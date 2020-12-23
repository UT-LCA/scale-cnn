// conv_golden.c
// Golden version of convolution algorithm used to validate synthesized version.
#include "global_defines.h"
#include "${lname}_common_defines.h"
#include <stdbool.h>

void conv_golden (
   data_t in_data[INPUT_RAM_SIZE],
   data_t out_data[OUTPUT_RAM_SIZE],
   data_t filter_data[FILTER_RAM_SIZE]
) {
  L1: for (int i = 0; i < OUTPUT_HEIGHT; i++) {
    L2: for (int j = 0; j < OUTPUT_WIDTH; j++) {
      L3: for (int k = 0; k < OUTPUT_CHANS; k++) {
        int out_data_idx = i*OUTPUT_WIDTH*OUTPUT_CHANS + j*OUTPUT_CHANS + k;
        data_t all_chans_val = 0;
        L4: for (int kk = 0; kk < INPUT_CHANS; kk++) {
          data_t val = 0;
          L5: for (int ii = 0; ii < FILTER_SIZE; ii++) {
            L6: for (int jj = 0; jj < FILTER_SIZE; jj++) {
              int filter_data_idx = (k  * FILTER_SIZE * FILTER_SIZE * INPUT_CHANS) +
                                    (ii * FILTER_SIZE * INPUT_CHANS) +
                                    (jj * INPUT_CHANS) +
                                    kk;
              int row_coord = (i*STRIDE) + ii - PAD;
              int col_coord = (j*STRIDE) + jj - PAD;
              bool is_padding = (row_coord < 0) || (row_coord >= INPUT_HEIGHT) ||
                                (col_coord < 0) || (col_coord >= INPUT_WIDTH);
              int in_data_idx = (row_coord * INPUT_WIDTH * INPUT_CHANS) +
                                (col_coord * INPUT_CHANS) +
                                kk;
              data_t in_data_elem = is_padding ? (data_t)0 : in_data[in_data_idx];
              val += in_data_elem * filter_data[filter_data_idx];
            }
          }
          all_chans_val += val;
        }
        out_data[out_data_idx] = all_chans_val;
      }
    }
  }
}

