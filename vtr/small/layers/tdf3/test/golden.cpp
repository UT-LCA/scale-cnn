// conv_golden.c
// Golden version of convolution algorithm used to validate synthesized version.
#include "global_defines.h"
#include "tdf3_common_defines.h"
#include <stdbool.h>
#include <math.h>

void conv_golden (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t adjustments[OUTPUT_CHANS][4]
) {
  L1: for (int i = 0; i < OUTPUT_HEIGHT; i++) {
    L2: for (int j = 0; j < OUTPUT_WIDTH; j++) {
      L3: for (int k = 0; k < OUTPUT_CHANS; k++) {
        int out_data_idx = i*OUTPUT_WIDTH*OUTPUT_CHANS + j*OUTPUT_CHANS + k;
        // To reduce the error of the expected values, use single-precision floating point
        // for all calculations and then convert to half-precision at the very end.
        float all_chans_val = 0;
        L4: for (int kk = 0; kk < INPUT_CHANS; kk++) {
          float val = 0;
          L5: for (int ii = 0; ii < FILTER_SIZE; ii++) {
            L6: for (int jj = 0; jj < FILTER_SIZE; jj++) {
              int row_coord = (i*STRIDE) + ii - PAD;
              int col_coord = (j*STRIDE) + jj - PAD;
              bool is_padding = (row_coord < 0) || (row_coord >= INPUT_HEIGHT) ||
                                (col_coord < 0) || (col_coord >= INPUT_WIDTH);
              data_t in_data_elem = is_padding ? (data_t)0 : in_data[row_coord][col_coord][kk];
              val += (float)in_data_elem * (float)filter_data[k][ii][jj][kk];
            }
          }
          all_chans_val += val;
        }
        float mean         = adjustments[k][0], 
              inv_sqrt_var = adjustments[k][1], 
              bias         = adjustments[k][2];
        float normalized = (all_chans_val - mean) * inv_sqrt_var;
        float biased     = normalized + bias;
        float activated  = fmax(0, biased); // ReLU activation
        out_data[i][j][k] = (data_t)activated;
      }
    }
  }
}

