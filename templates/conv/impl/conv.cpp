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

// Reads filters into an internal buffer (weight_vec)
void ${lname}_readFilters (
   data_t filter_data[FILTER_RAM_SIZE],
   int k,
   data_t weight_vec[VECTOR_SIZE]
) {
   FL4: for (int ii = 0; ii < FILTER_SIZE; ++ii) {
      FL5: for (int jj = 0; jj < FILTER_SIZE; ++jj) {
         FL6: for (int kk = 0; kk < INPUT_CHANS; ++kk) {
            int filter_data_idx = (k  * FILTER_SIZE * FILTER_SIZE * INPUT_CHANS) +
                                  (ii * FILTER_SIZE * INPUT_CHANS) +
                                  (jj * INPUT_CHANS) +
                                  kk;
            int vec_idx = kk + (INPUT_CHANS*jj) + (INPUT_CHANS*FILTER_SIZE*ii);
            weight_vec[vec_idx] = filter_data[filter_data_idx];
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
void ${lname}_writeOutput(
   data_t outputElem,
   uram_o out_data[OUTPUT_RAM_SIZE]
) {
   static int outputCount = 0;
   static int outputIdx   = 0;
   static uram_o outputRow;
   #pragma HLS array_partition variable=outputRow.d complete
   // Ideally, we wouldn't need this for loop and could just do a single
   // write to outputRow.d[outputCount]. However, for some strange reason,
   // this causes the synthesis time of the layer to explode (from less than
   // a minute to over 20 minutes). So writing it this way instead.
   for (int x = 0; x < 4; x++) {
      #pragma HLS unroll
      outputRow.d[x] = (x == outputCount) ? outputElem : outputRow.d[x];
   }
   outputCount++;
   if (outputCount == $output_words_per_uram_row) {
      outputCount = 0;
      out_data[outputIdx] = outputRow;
      outputIdx++;
   }
}

// Calculates the dot product of the ifmap and weight vectors.
// All this stage does is multiply the numbers together into an array of 
// products. The accumulation is handled by separate stages.
void ${lname}_dot_product (
   data_t ifmap_vec[VECTOR_SIZE],
   data_t weight_vec[VECTOR_SIZE],
   data_t products[VECTOR_SIZE]
) { 
   // Sometimes the synthesizer is unable to figure out there are no 
   // WAW dependencies for when we utilize both write ports of one BRAM.
   // I'm not sure why this only sometimes happens and sometimes doesn't.
   // But luckily we can explicitly tell it there are no dependencies.
   #pragma HLS dependence variable=products inter WAW false
   #pragma HLS dependence variable=products intra WAW false
   DP_LOOP: for (int p = 0; p < VECTOR_SIZE; p++) {
      products[p] = ifmap_vec[p] * weight_vec[p];
   }
}

//////////////////////////////////////////////////////////////
//  ACCUMULATION FUNCTIONS
//////////////////////////////////////////////////////////////

$accum_functions


// Function that keeps track of indices i,j,k for the top loop
void ${lname}_get_next_ijk (int indices[3]) {
   static int i = 0;
   static int j = 0;
   static int k = 0;
   indices[0] = i;
   indices[1] = j;
   indices[2] = k;
   k++;
   if (k == OUTPUT_CHANS) {
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
   data_t filter_data[FILTER_RAM_SIZE]
) {
   // Ideally, this single for loop would be split into three nested loops like this,
   // where the dataflow directive would be applied to L3:
   // 
   // L1: for (int i = 0; i < OUTPUT_HEIGHT; i++) {
   //    L2: for (int j = 0; j < OUTPUT_WIDTH; j++) {
   //       L3: for (int k = 0; k < OUTPUT_CHANS; k++) {
   //          (loop body)
   //       }
   //    }
   // }
   //
   // While this does technically work with the dataflow optimization, the synthesizer
   // is unable to properly flatten the three loops such that all calls to the dataflow
   // pipeline occur in one single contiguous stream. Instead, only OUTPUT_CHANS calls 
   // are made in a row, and then L2 cannot begin its next iteration until the dataflow
   // pipeline is completely empty. Telling the synthesizer to explicitly flatten the loops
   // only makes the problem worse and causes the dataflow optimization to fail entirely.
   //
   // So instead, we must explicitly flatten the loops in the C code itself. The "get_next_ijk"
   // function will keep track of what the values of i,j,k would be if the loops were written 
   // as shown above.
   TOP_LOOP: for (int f = 0; f < NUM_OUTPUTS; f++) {
      data_t ifmap_vec[VECTOR_SIZE];
      data_t weight_vec[VECTOR_SIZE];
      data_t products[VECTOR_SIZE];
      int indices[3];
      #pragma HLS array_partition variable=indices complete
      ${lname}_get_next_ijk(indices);
      int i_int = indices[0];
      int j_int = indices[1];
      int k_int = indices[2];
      // FOR EACH OUTPUT ELEMENT:
      // read the inputs needed for this output element
      ${lname}_readInputs(in_data, i_int, j_int, ifmap_vec);
      // read the filters needed for this output element
      ${lname}_readFilters(filter_data, k_int, weight_vec);
      // Calculate the dot product, result is returned as a list of partial sums
      ${lname}_dot_product(ifmap_vec, weight_vec, products);
      // Accumulate the products
$accum_function_calls
      // Store the final output
      ${lname}_writeOutput(final_sum, out_data);
   }
}
