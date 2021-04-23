// Testbench for tdf5
#include "global_defines.h"
#include "tb_utils.h"
#include "tdf5_common_defines.h"
#include <stdio.h>

void tdf5 (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t adjustments[OUTPUT_CHANS][4]
);

void convmax_golden (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t adjustments[OUTPUT_CHANS][4]
);

int main() {

   printf("Beginning functional validation.\n");
   printf("Generating random inputs.\n");
   fflush(stdout);

   // Generate random inputs and filters (flattened)
   data_t ifmaps[NUM_INPUTS];
   data_t filters[FILTER_RAM_SIZE];
   data_t adjustments[OUTPUT_CHANS][4];
   gen_random_inputs(ifmaps, NUM_INPUTS);
   gen_random_filters(filters, FILTER_RAM_SIZE);
   gen_random_adjustments(adjustments, OUTPUT_CHANS);

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
   data_t filters_4d[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
   for (int ochan = 0; ochan < OUTPUT_CHANS; ochan++) {
      for (int i = 0; i < FILTER_SIZE; i++) {
         for (int j = 0; j < FILTER_SIZE; j++) {
            for (int k = 0; k < INPUT_CHANS; k++) {
               int idx = k + INPUT_CHANS * (j + FILTER_SIZE * (i + FILTER_SIZE * ochan));
               filters_4d[ochan][i][j][k] = filters[idx];
            }
         }
      }
   }

   // Run the golden function and the synthesized function with the same inputs.
   data_t ofmaps_synth_3d[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS];
   data_t ofmaps_gold_3d[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS];
   printf("Running synthesized function...\n");
   fflush(stdout);
   tdf5(ifmaps_3d, ofmaps_synth_3d, filters_4d, adjustments);
   printf("Running golden comparison function...\n");
   fflush(stdout);
   convmax_golden(ifmaps_3d, ofmaps_gold_3d, filters_4d, adjustments);

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

   int res = compare_expected_vs_actual(ofmaps_gold, ofmaps_synth, NUM_OUTPUTS, INPUT_CHANS); 
   if (res == 0) printf("Validation successful.\n");
   return res;
}
