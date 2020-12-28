#include "global_defines.h"
#include "${lname}_impl_defines.h"
#include <stdbool.h>

// Reads input feature maps into an internal buffer (ifmap_vec)
void ${lname}_readInputs ( 
   uram_i in_data[INPUT_RAM_SIZE],
   int i, int j,
   data_t ifmap_vec[VECTOR_SIZE] ) {
   IL4: for (int ii = 0; ii < FILTER_SIZE; ++ii) {
      IL5: for (int jj = 0; jj < FILTER_SIZE; ++jj) {
         int row_coord = (i*STRIDE) + ii - PAD;
         int col_coord = (j*STRIDE) + jj - PAD;
         bool is_padding = (row_coord < 0) || (row_coord >= INPUT_HEIGHT) ||
                           (col_coord < 0) || (col_coord >= INPUT_WIDTH);
         int input_pixel_base = ((row_coord * INPUT_WIDTH) + col_coord) * (INPUT_CHANS / $input_words_per_uram_row);
         int filter_pixel_base = (INPUT_CHANS*jj) + (INPUT_CHANS*FILTER_SIZE*ii);
         IL6: for (int kk = 0; kk < INPUT_CHANS / $input_words_per_uram_row; kk++) {
            int in_data_idx = input_pixel_base + kk;
            IL7: for (int u = 0; u < $input_words_per_uram_row; ++u) {
               int vec_idx = filter_pixel_base + (kk*$input_words_per_uram_row) + u;
               ifmap_vec[vec_idx] = is_padding ? (data_t)0 : in_data[in_data_idx].d[u];
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
// This function receives one word per function call, and packs the words
// together before writing them to the output URAM. Because of this, the write
// operation only occurs once every 3 or 4 calls. For this reason, this function
// must use static variables to remember how many words are currently stored in 
// the 
void ${lname}_writeOutputs(
   data_t outputs[OCHAN_SCALE_FACTOR],
   uram_o out_data[OUTPUT_RAM_SIZE]
) {
   static int outputCount = 0;
   static int outputIdx   = 0;
   static uram_o outputRow;
   #pragma HLS array_partition variable=outputRow.d complete
   // If this function ever becomes a bottleneck, I should be able
   // to unroll it by output_words_per_uram_row
   for (int o = 0; o < OCHAN_SCALE_FACTOR; o++) {
      // Ideally, we wouldn't need this for loop and could just do a single
      // write to outputRow.d[outputCount]. However, for some strange reason,
      // this causes the synthesis time of the layer to explode (from less than
      // a minute to over 20 minutes). So writing it this way instead.
      for (int x = 0; x < 4; x++) {
         #pragma HLS unroll
         outputRow.d[x] = (x == outputCount) ? outputs[o] : outputRow.d[x];
      }
      outputCount++;
      if (outputCount == $output_words_per_uram_row) {
         outputCount = 0;
         out_data[outputIdx] = outputRow;
         outputIdx++;
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
   #pragma HLS dependence variable=products inter WAW false
   #pragma HLS dependence variable=products intra WAW false
   DP_LOOP: for (int p = 0; p < VECTOR_SIZE; p++) {
      DP_OCHANS: for (int oc = 0; oc < OCHAN_SCALE_FACTOR; oc++) { 
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
   uram_i in_data[INPUT_RAM_SIZE],
   uram_o out_data[OUTPUT_RAM_SIZE],
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
   TOP_LOOP: for (int f = 0; f < NUM_OUTPUTS / OCHAN_SCALE_FACTOR; f++) {
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
      OCHAN_LOOP: for (int o = 0; o < OCHAN_SCALE_FACTOR; o++) {
         #pragma HLS unroll
$accum_function_calls
         outputs[o] = final_sum;
      }
      ${lname}_writeOutputs(outputs, out_data);
   }
}

// Top-level wrapper function for $lname
void ${lname}_top() {
   uram_i in_data[INPUT_RAM_SIZE];
   uram_o out_data[OUTPUT_RAM_SIZE];
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER];
   ${lname}(in_data, out_data, filter_data);
}

