#include "global_defines.h"
#include "${lname}_impl_defines.h"
#include <stdbool.h>
#include <assert.h>

// Reads input feature maps into an internal buffer (ifmap_vec)
// The assertions tell the compiler the upper bounds of the index
// variables which enable it to make major optimizations on the 
// arithmetic logic.
void ${lname}_readInputs ( 
   data_t in_data[INPUT_RAM_SIZE],
   int i, int j,
   data_t ifmap_vec[VECTOR_SIZE] ) {
   IL4: for (int ii = 0; ii < FILTER_SIZE; ++ii) {
      IL5: for (int jj = 0; jj < FILTER_SIZE; ++jj) {
         int row_coord = (i*STRIDE) + ii - PAD;
         int col_coord = (j*STRIDE) + jj - PAD;
         bool is_padding = (row_coord < 0) || (row_coord >= INPUT_HEIGHT) ||
                           (col_coord < 0) || (col_coord >= INPUT_WIDTH);
         int input_pixel_base = ((row_coord * INPUT_WIDTH) + col_coord) * INPUT_CHANS_PADDED;
         int filter_pixel_base = ((ii*FILTER_SIZE) + jj) * INPUT_CHANS;
         IL6: for (int kk = 0; kk < INPUT_CHANS; kk++) {
            int in_data_idx = is_padding ? 0 : input_pixel_base + kk;
            int vec_idx     = filter_pixel_base + kk;
            assert(in_data_idx < INPUT_RAM_SIZE);
            assert(vec_idx < VECTOR_SIZE);
            ifmap_vec[vec_idx] = is_padding ? (data_t)0 : in_data[in_data_idx];
         }
      }
   }
}

// Reads filters into internal buffers (weight_vecs)
// It reads all the elements of $ochan_scale_factor filter(s)
// Like with readInputs the assertions enable compiler optimizations
void ${lname}_readFilters (
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER],
   int k,
   data_t weight_vecs[OCHAN_SCALE_FACTOR][VECTOR_SIZE]
) {
   FL4: for (int ii = 0; ii < FILTER_SIZE; ++ii) {
      FL5: for (int jj = 0; jj < FILTER_SIZE; ++jj) {
         int idx_base = (ii*FILTER_SIZE + jj) * INPUT_CHANS;
         FL6: for (int kk = 0; kk < INPUT_CHANS; ++kk) {
            int idx = idx_base + kk;
            assert(idx < WORDS_PER_FILTER);
            FL7: for (int o = 0; o < OCHAN_SCALE_FACTOR; o++) {
               // This loop should always be unrolled completely.
               #pragma HLS unroll
               int ochan = (k*OCHAN_SCALE_FACTOR) + o;
               assert(ochan < OUTPUT_CHANS);
               weight_vecs[o][idx] = filter_data[ochan][idx];
            }
         }
      }
   }
}  


// Function for writing output data to the output URAMs.
// This function receives OCHAN_SCALE_FACTOR words per function call, and packs 
// the words together before writing them to the output URAM. Each URAM row will 
// hold 4 elements. In this version of the function, OCHAN_SCALE_FACTOR is not a
// multiple of 4 (and may be less than 4). For this reason, this function must use 
// static variables to store partial URAM rows so that it only writes to the URAM 
// when an entire row is ready. If OCHAN_SCALE_FACTOR is less than 4, then not every 
// call to this function will result in an actual write to the URAMs.
//
// Due to the nature of how static variables are accessed, the outer loop cannot be
// pipelined. This is not a big deal because in most cases, OCHAN_SCALE_FACTOR will
// be either 1 or 2, so pipelining would not even make sense.
void ${lname}_writeOutputs_unaligned(
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t out_data[OUTPUT_RAM_SIZE]
) {
   static int outputCount = 0;
   static int outputIdx   = 0;
   static data_t outputRow[$output_words_per_uram_row];
   #pragma HLS array_partition variable=outputRow complete
   for (int o = 0; o < OCHAN_SCALE_FACTOR; o++) {
      outputRow[outputCount] = outputs[o];
      outputCount++;
      if (outputCount == $output_words_per_uram_row) {
         outputCount = 0;
         for (int w = 0; w < $output_words_per_uram_row; w++) {
            #pragma HLS unroll
            out_data[(outputIdx*$output_words_per_uram_row) + w] = outputRow[w];
         }
         outputIdx++;
      }
   }
}

// This is the aligned version of writeOutputs
// In this version, we are guaranteed that OCHAN_SCALE_FACTOR is either exactly
// 4 or a multiple of 4. This enables us to avoid the use of static variables
// outputRow and outputCount and process 4 words at once in the outer loop.
void ${lname}_writeOutputs_aligned(
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t out_data[OUTPUT_RAM_SIZE]
) {
   static int row_count = 0;
   for (int o = 0; o < (OCHAN_SCALE_FACTOR/$output_words_per_uram_row); o++) {
      for (int w = 0; w < $output_words_per_uram_row; w++) {
         #pragma HLS unroll
         int outputs_idx  = (o*$output_words_per_uram_row) + w;
         int out_data_idx = (row_count*$output_words_per_uram_row) + w;
         assert(outputs_idx < OCHAN_SCALE_FACTOR);
         assert(out_data_idx < OUTPUT_RAM_SIZE);
         out_data[out_data_idx] = outputs[outputs_idx];
      }
      row_count++;
   }
}


// Calculates the dot product of the ifmap and weight vectors.
// All this stage does is multiply the numbers together into an array of 
// products. The accumulation is handled by separate stages.
// This stage also handles multiplying the inputs by different filters
// at the same time. This needs to be handled in the inner loop of the 
// function so that the synthesizer can take advantage of only reading 
// each input data once.
void ${lname}_dot_product (
   data_t ifmap_vec[VECTOR_SIZE],
   data_t weight_vecs[OCHAN_SCALE_FACTOR][VECTOR_SIZE],
   data_t products[OCHAN_SCALE_FACTOR][VECTOR_SIZE]
) { 
   DP_OUTER: for (int p = 0; p < VECTOR_SIZE; p++) {
      DP_INNER: for (int oc = 0; oc < OCHAN_SCALE_FACTOR; oc++) { 
         products[oc][p] = ifmap_vec[p] * weight_vecs[oc][p];
      }
   }
}

//////////////////////////////////////////////////////////////
//  ACCUMULATION FUNCTIONS
//////////////////////////////////////////////////////////////

$accum_functions


// Function that keeps track of indices i,j,k for the top loop
// i and j are the output row and column coordinates, respectively
// k represents the output channel, but not directly. It actually 
// represents the group of output channels, since we can parallelize
// mutliple output channels for the same output XY coordinate. 
// For example, if OCHAN_SCALE_FACTOR = 4 (meaning we process 4 output channels
// at the same time), then k = 1 represents output channels 4,5,6,7.
void ${lname}_get_next_ijk (int indices[3]) {
   static int i = 0;
   static int j = 0;
   static int k = 0;
   indices[0] = i;
   indices[1] = j;
   indices[2] = k;
   k++;
   if (k == OUTPUT_CHANS / OCHAN_SCALE_FACTOR) {
      k = 0;
      j++;
      if (j == OUTPUT_WIDTH) {
         j = 0;
         i++;
         if (i == OUTPUT_HEIGHT) {
            i = 0;
         }
      }
   }
}


void $lname (
   data_t in_data[INPUT_RAM_SIZE],
   data_t out_data[OUTPUT_RAM_SIZE],
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER]
) {
   // Ideally, this single for loop would be split into three nested loops like this,
   // where the dataflow directive would be applied to L3:
   // 
   // L1: for (int i = 0; i < OUTPUT_HEIGHT; i++) {
   //    L2: for (int j = 0; j < OUTPUT_WIDTH; j++) {
   //       L3: for (int k = 0; k < OUTPUT_CHANS / OCHAN_SCALE_FACTOR; k++) {
   //          (loop body)
   //       }
   //    }
   // }
   //
   // While this does technically work with the dataflow optimization, the synthesizer
   // is unable to properly flatten the three loops such that all calls to the dataflow
   // pipeline occur in one single contiguous stream. Instead, only (L3 trip count) calls 
   // are made in a row, and then L2 cannot begin its next iteration until the dataflow
   // pipeline is completely empty. Telling the synthesizer to explicitly flatten the loops
   // only makes the problem worse and causes the dataflow optimization to fail entirely.
   //
   // So instead, we must explicitly flatten the loops in the C code itself. The "get_next_ijk"
   // function will keep track of what the values of i,j,k would be if the loops were written 
   // as shown above.
   //
   // TODO: Figure out if this is fixed in Vitis.
   TOP_LOOP: for (int f = 0; f < TOP_LOOP_ITERATIONS; f++) {
      #pragma HLS stable variable=in_data
      #pragma HLS stable variable=filter_data
      data_t ifmap_vec[VECTOR_SIZE];
      data_t weight_vecs[OCHAN_SCALE_FACTOR][VECTOR_SIZE];
      data_t products[OCHAN_SCALE_FACTOR][VECTOR_SIZE];
      data_t outputs[OCHAN_SCALE_FACTOR];
      #pragma HLS array_partition variable=outputs complete
      int indices[3];
      #pragma HLS array_partition variable=indices complete
      ${lname}_get_next_ijk(indices);
      int i_int = indices[0];
      int j_int = indices[1];
      int k_int = indices[2];
      // FOR EACH OUTPUT ELEMENT:
      //  - Read the convolution window of inputs
      //  - Read the filters
      //  - Perform element-wise multiplication of the inputs and weights
      //  - Accumulate the results
      //  - Write the outputs.
      //
      //  Note that we can process multiple filters / output channels at the same time.
      ${lname}_readInputs(in_data, i_int, j_int, ifmap_vec);
      ${lname}_readFilters(filter_data, k_int, weight_vecs);
      ${lname}_dot_product(ifmap_vec, weight_vecs, products);
$accum_function_calls
      ${lname}_writeOutputs_${writeFuncType}(outputs, out_data);
   }
}

// Top-level wrapper function for $lname
void ${lname}_top() {
   data_t in_data[INPUT_RAM_SIZE];
   data_t out_data[OUTPUT_RAM_SIZE];
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER];
   ${lname}(in_data, out_data, filter_data);
}

