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
         int input_pixel_base = (row_coord * INPUT_WIDTH * INPUT_CHANS) +
                                (col_coord * INPUT_CHANS);
         int filter_pixel_base = (INPUT_CHANS*jj) + (INPUT_CHANS*FILTER_SIZE*ii);
         IL6: for (int kk = 0; kk < INPUT_CHANS; ++kk) {
            int in_data_idx = input_pixel_base  + kk;
            int vec_idx     = filter_pixel_base + kk;
            data_t in_data_elem = is_padding ? 0 : in_data[in_data_idx];
            ifmap_vec[vec_idx] = in_data_elem;
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


// Calculates the dot product of the ifmap and weight vectors.
// All this stage does is multiply the numbers together into an array of 
// products. The accumulation is handled by separate stages.
void ${lname}_dot_product (
   data_t ifmap_vec[VECTOR_SIZE],
   data_t weight_vec[VECTOR_SIZE],
   data_t products[VECTOR_SIZE]
) { 
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
   data_t in_data[INPUT_RAM_SIZE],
   data_t out_data[OUTPUT_RAM_SIZE],
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
   // function will take in the current iteration of this loop (f) and spit out what the 
   // values of i,j,k would be if the loops were written as shown above.
   TOP_LOOP: for (int f = 0; f < (OUTPUT_HEIGHT * OUTPUT_WIDTH * OUTPUT_CHANS); f++) {
      data_t ifmap_vec[VECTOR_SIZE];
      data_t weight_vec[VECTOR_SIZE];
      data_t products[VECTOR_SIZE];
      int indices[3];
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
      out_data[f] = final_sum;
   }
}
