// golden.cpp
// Golden version of fused conv-conv algorithm used to validate synthesized version.
#include "global_defines.h"
#include "${lname}_common_defines.h"
#include <stdbool.h>

void convconv_golden (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t l1_filter_data[L1_OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t l2_filter_data[OUTPUT_CHANS][L1_OUTPUT_CHANS],
   data_t l1_adjustments[L1_OUTPUT_CHANS][4],
   data_t l2_adjustments[OUTPUT_CHANS][4]
) {
  float intermediate_fmaps[OUTPUT_HEIGHT][OUTPUT_WIDTH][L1_OUTPUT_CHANS];
  // First of the two layers fused together
  L1_1: for (int i = 0; i < OUTPUT_HEIGHT; i++) {
    L2_1: for (int j = 0; j < OUTPUT_WIDTH; j++) {
      L3_1: for (int k = 0; k < L1_OUTPUT_CHANS; k++) {
        // To reduce the error of the expected values, use single-precision floating point
        // for all calculations and then convert to half-precision at the very end.
        float all_chans_val = 0;
        L4_1: for (int kk = 0; kk < INPUT_CHANS; kk++) {
          float val = 0;
          L5_1: for (int ii = 0; ii < FILTER_SIZE; ii++) {
            L6_1: for (int jj = 0; jj < FILTER_SIZE; jj++) {
              int row_coord = (i*STRIDE) + ii - PAD;
              int col_coord = (j*STRIDE) + jj - PAD;
              bool is_padding = (row_coord < 0) || (row_coord >= INPUT_HEIGHT) ||
                                (col_coord < 0) || (col_coord >= INPUT_WIDTH);
              data_t in_data_elem = is_padding ? (data_t)0 : in_data[row_coord][col_coord][kk];
              val += (float)in_data_elem * (float)l1_filter_data[k][ii][jj][kk];
            }
          }
          all_chans_val += val;
        }
        // batch normalization / bias / adjustment
        float mean         = l1_adjustments[k][0], 
              inv_sqrt_var = l1_adjustments[k][1], 
              bias         = l1_adjustments[k][2];
        float normalized = (all_chans_val - mean) * inv_sqrt_var;
        float biased     = normalized + bias;
        float activated  = fmax(0, biased); // ReLU activation
        intermediate_fmaps[i][j][k] = activated;
      }
      // Now multiply the 1x1xL1_OUTPUT_CHANS elements against OUTPUT_CHANS
      // 1x1 filters of the second half of the fused conv-conv layer to get the final
      // 1x1xOUTPUT_CHANS output elements.
      L3_2: for (int oc = 0; oc < OUTPUT_CHANS; oc++) {
         float val = 0;
         L4_2: for (int ic = 0; ic < L1_OUTPUT_CHANS; ic++) {
            val += (float)l2_filter_data[oc][ic] * intermediate_fmaps[i][j][ic];
         }
         // batch normalization / bias / adjustment
         float mean         = l2_adjustments[oc][0], 
               inv_sqrt_var = l2_adjustments[oc][1], 
               bias         = l2_adjustments[oc][2];
         float normalized = (val - mean) * inv_sqrt_var;
         float biased     = normalized + bias;
         float activated  = fmax(0, biased); // ReLU activation
         out_data[i][j][oc] = (data_t)activated;
      }
    }
  }
}

