#include "global_defines.h"
#include "${lname}_impl_defines.h"
#include <stdbool.h>

// Reads input feature maps into an internal buffer (ifmap_vec)
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
         int input_pixel_base = ((row_coord * INPUT_WIDTH) + col_coord) * INPUT_CHANS;
         int filter_pixel_base = ((ii*FILTER_SIZE) + jj) * INPUT_CHANS;
         IL6: for (int kk = 0; kk < INPUT_CHANS / $input_words_per_uram_row; kk++) {
            IL7: for (int u = 0; u < $input_words_per_uram_row; ++u) {
               int offset = (kk*$input_words_per_uram_row) + u;
               int in_data_idx = input_pixel_base  + offset;
               int vec_idx     = filter_pixel_base + offset;
               ifmap_vec[vec_idx] = is_padding ? (data_t)0 : in_data[in_data_idx];
            }
         }
      }
   }
}

// Reads filters into internal buffers (weight_vecs)
// It reads all the elements of $ochan_scale_factor filter(s)
void ${lname}_readFilters (
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER],
   int k,
   data_t weight_vecs[OCHAN_SCALE_FACTOR][VECTOR_SIZE]
) {
   FL4: for (int ii = 0; ii < FILTER_SIZE; ++ii) {
      FL5: for (int jj = 0; jj < FILTER_SIZE; ++jj) {
         FL6: for (int kk = 0; kk < INPUT_CHANS; ++kk) {
            int filter_data_idx = (ii * FILTER_SIZE * INPUT_CHANS) +
                                  (jj * INPUT_CHANS) +
                                  kk;
            int vec_idx = kk + (INPUT_CHANS*jj) + (INPUT_CHANS*FILTER_SIZE*ii);
            FL7: for (int o = 0; o < OCHAN_SCALE_FACTOR; o++) {
               // This loop should always be unrolled completely.
               #pragma HLS unroll
               int ochan = (k*OCHAN_SCALE_FACTOR) + o;
               weight_vecs[o][vec_idx] = filter_data[ochan][filter_data_idx];
            }
         }
      }
   }
}  


// Function for writing output data to the output URAMs.
// This function receives OCHAN_SCALE_FACTOR words per function call, and packs 
// the words together before writing them to the output URAM. Each URAM row will 
// hold either 3 or 4 elements. OCHAN_SCALE_FACTOR may be less than, equal to, or 
// greater than the number of elements per row. For this reason, this function
// must use static variables to store partial URAM rows so that it only writes
// to the URAM when an entire row is ready. If OCHAN_SCALE_FACTOR is less than
// the number of elements per URAM row, the not every call to this function will
// result in an actual write to the URAMs.
void ${lname}_writeOutputs(
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
            out_data[outputIdx + w] = outputRow[w];
         }
         outputIdx += $output_words_per_uram_row;
      }
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
   // Sometimes the synthesizer is unable to figure out there are no 
   // WAW dependencies for when we utilize both write ports of one BRAM.
   // I'm not sure why this only sometimes happens and sometimes doesn't.
   // But luckily we can explicitly tell it there are no dependencies.
   // TODO: figure out if this is still necessary with Vitis.
   #pragma HLS dependence variable=products inter WAW false
   #pragma HLS dependence variable=products intra WAW false
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
      ${lname}_writeOutputs(outputs, out_data);
   }
}

// Top-level wrapper function for $lname
void ${lname}_top() {
   data_t in_data[INPUT_RAM_SIZE];
   data_t out_data[OUTPUT_RAM_SIZE];
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER];
   ${lname}(in_data, out_data, filter_data);
}

