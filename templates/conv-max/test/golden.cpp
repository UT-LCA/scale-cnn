// Golden version of convolution/maxpool algorithm used to validate synthesized version.
#include "global_defines.h"
#include "${lname}_common_defines.h"
#include <stdbool.h>

void convmax_golden (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS]
) {
  static const int OU_HEIGHT = INPUT_HEIGHT + (2*PAD) - FILTER_SIZE + 1;
  static const int OU_WIDTH  = INPUT_WIDTH  + (2*PAD) - FILTER_SIZE + 1;
  data_t out_data_unpooled[OU_HEIGHT][OU_WIDTH][OUTPUT_CHANS];
  // First do the standard convolution algorithm
  L1: for (int i = 0; i < OU_HEIGHT; i++) {
    L2: for (int j = 0; j < OU_WIDTH; j++) {
      L3: for (int k = 0; k < OUTPUT_CHANS; k++) {
        int out_data_idx = i*OU_WIDTH*OUTPUT_CHANS + j*OUTPUT_CHANS + k;
        data_t all_chans_val = 0;
        L4: for (int kk = 0; kk < INPUT_CHANS; kk++) {
          data_t val = 0;
          L5: for (int ii = 0; ii < FILTER_SIZE; ii++) {
            L6: for (int jj = 0; jj < FILTER_SIZE; jj++) {
              int row_coord = (i*STRIDE) + ii - PAD;
              int col_coord = (j*STRIDE) + jj - PAD;
              bool is_padding = (row_coord < 0) || (row_coord >= INPUT_HEIGHT) ||
                                (col_coord < 0) || (col_coord >= INPUT_WIDTH);
              data_t in_data_elem = is_padding ? (data_t)0 : in_data[row_coord][col_coord][kk];
              val += in_data_elem * filter_data[k][ii][jj][kk];
            }
          }
          all_chans_val += val;
        }
        out_data_unpooled[i][j][k] = all_chans_val;
      }
    }
  }
   // Now do the pooling
   for (int ch = 0; ch < OUTPUT_CHANS; ch++) {
      for (int r = 0; r < OUTPUT_HEIGHT; r++) {
         for (int c = 0; c < OUTPUT_WIDTH; c++) {
            data_t max = 0;
            for (int pr = 0; pr < POOLING_FACTOR; pr++) {
               for (int pc = 0; pc < POOLING_FACTOR; pc++) {
                  data_t val = out_data_unpooled[r*POOLING_FACTOR+pr][c*POOLING_FACTOR+pc][ch];
                  if ((pr == 0 && pc == 0) || val > max) max = val;
               }
            }
            out_data[r][c][ch] = max;
         }
      }
   }
}

