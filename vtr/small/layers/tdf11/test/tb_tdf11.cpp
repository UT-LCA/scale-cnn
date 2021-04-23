// Testbench for tdf11
#include "global_defines.h"
#include "tb_utils.h"
#include "tdf11_common_defines.h"
#include <stdio.h>

void tdf11 (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t l1_filter_data[L1_OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t l2_filter_data[OUTPUT_CHANS][L1_OUTPUT_CHANS],
   data_t l1_adjustments[L1_OUTPUT_CHANS][4],
   data_t l2_adjustments[OUTPUT_CHANS][4]
);

void convconv_golden (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t l1_filter_data[L1_OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t l2_filter_data[OUTPUT_CHANS][L1_OUTPUT_CHANS],
   data_t l1_adjustments[L1_OUTPUT_CHANS][4],
   data_t l2_adjustments[OUTPUT_CHANS][4]
);

int main() {

   printf("Beginning functional validation.\n");
   printf("Generating random inputs.\n");
   fflush(stdout);

   // Generate random inputs and filters (flattened)
   data_t ifmaps[NUM_INPUTS];
   data_t l1_filters[L1_FILTER_RAM_SIZE];
   data_t l2_filters[L2_FILTER_RAM_SIZE];
   data_t l1_adjustments[L1_OUTPUT_CHANS][4];
   data_t l2_adjustments[OUTPUT_CHANS][4];

   gen_random_inputs(ifmaps, NUM_INPUTS);
   gen_random_filters(l1_filters, L1_FILTER_RAM_SIZE);
   gen_random_filters(l2_filters, L2_FILTER_RAM_SIZE);
   gen_random_adjustments(l1_adjustments, L1_OUTPUT_CHANS);
   gen_random_adjustments(l2_adjustments, OUTPUT_CHANS);

   // Copy the flattened 1D ifmaps to the 3D array
   data_t ifmaps_3d[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED];
   for (int i = 0; i < INPUT_HEIGHT; i++) {
      for (int j = 0; j < INPUT_WIDTH; j++) {
         for (int k = 0; k < INPUT_CHANS; k++) {
            int idx = k + INPUT_CHANS * (j + INPUT_WIDTH * i);
            ifmaps_3d[i][j][k] = ifmaps[idx];
         }
      }
   }

   // Copy the flattened 1D filters to the 4D array
   data_t l1_filters_4d[L1_OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
   for (int ochan = 0; ochan < L1_OUTPUT_CHANS; ochan++) {
      for (int i = 0; i < FILTER_SIZE; i++) {
         for (int j = 0; j < FILTER_SIZE; j++) {
            for (int k = 0; k < INPUT_CHANS; k++) {
               int idx = k + INPUT_CHANS * (j + FILTER_SIZE * (i + FILTER_SIZE * ochan));
               l1_filters_4d[ochan][i][j][k] = l1_filters[idx];
            }
         }
      }
   }

   // Do the same for the L2 filters
   data_t l2_filters_2d[OUTPUT_CHANS][L1_OUTPUT_CHANS];
   for (int ochan = 0; ochan < OUTPUT_CHANS; ochan++) {
      for (int k = 0; k < L1_OUTPUT_CHANS; k++) {
         int idx = k + L1_OUTPUT_CHANS * ochan;
         l2_filters_2d[ochan][k] = l2_filters[idx];
      }
   }

   // Run the golden function and the synthesized function with the same inputs.
   data_t ofmaps_synth_3d[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS];
   data_t ofmaps_gold_3d[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS];
   printf("Running synthesized function...\n");
   fflush(stdout);
   tdf11(ifmaps_3d, ofmaps_synth_3d, l1_filters_4d, l2_filters_2d, 
          l1_adjustments, l2_adjustments);
   printf("Running golden comparison function...\n");
   fflush(stdout);
   convconv_golden(ifmaps_3d, ofmaps_gold_3d, l1_filters_4d, l2_filters_2d, 
                   l1_adjustments, l2_adjustments);

   // Copy the 3d output arrays to 1d arrays for the compare function
   data_t ofmaps_synth[NUM_OUTPUTS];
   data_t ofmaps_gold[NUM_OUTPUTS];
   for (int i = 0; i < OUTPUT_HEIGHT; i++) {
      for (int j = 0; j < OUTPUT_WIDTH; j++) {
         for (int k = 0; k < OUTPUT_CHANS; k++) {
            int idx = k + OUTPUT_CHANS * (j + OUTPUT_WIDTH * i);
            ofmaps_synth[idx] = ofmaps_synth_3d[i][j][k];
            ofmaps_gold[idx]  = ofmaps_gold_3d[i][j][k];
         }
      }
   }


   printf("Comparing expected vs. actual values.\n");
   fflush(stdout);

   int res = compare_expected_vs_actual(ofmaps_gold, ofmaps_synth, NUM_OUTPUTS, L1_OUTPUT_CHANS); 
   if (res == 0) printf("Validation successful.\n");
   return res;
}
