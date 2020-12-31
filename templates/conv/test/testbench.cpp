// Testbench for $lname
#include "global_defines.h"
#include "tb_utils.h"
#include "${lname}_common_defines.h"
#include <stdio.h>

void $lname (
   data_t in_data[INPUT_RAM_SIZE],
   data_t out_data[OUTPUT_RAM_SIZE],
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER]
);

void conv_golden (
   data_t in_data[NUM_INPUTS],
   data_t out_data[NUM_OUTPUTS],
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER]
);

int main() {

   printf("Beginning functional validation.\n");
   printf("Generating random inputs.\n");
   fflush(stdout);

   // Generate random inputs and filters
   data_t ifmaps[NUM_INPUTS];
   data_t filters[OUTPUT_CHANS][WORDS_PER_FILTER];
   gen_random_inputs(ifmaps, NUM_INPUTS);
   gen_random_filters<OUTPUT_CHANS, WORDS_PER_FILTER>(filters);

   // Copy the ifmaps to the padded version for the synthesized function.
   data_t ifmaps_padded[INPUT_RAM_SIZE];
   for (int i = 0; i < INPUT_HEIGHT; i++) {
      for (int j = 0; j < INPUT_WIDTH; j++) {
         for (int k = 0; k < INPUT_CHANS; k++) {
            int padded_idx     = k + INPUT_CHANS_PADDED * (j + INPUT_WIDTH * i);
            int non_padded_idx = k + INPUT_CHANS        * (j + INPUT_WIDTH * i);
            ifmaps_padded[padded_idx] = ifmaps[non_padded_idx];
         }
      }
   }

   // Run the golden function and the synthesized function with the same inputs.
   data_t ofmaps_synth[NUM_OUTPUTS];
   data_t ofmaps_gold[NUM_OUTPUTS];
   printf("Running synthesized function...\n");
   fflush(stdout);
   $lname(ifmaps_padded, ofmaps_synth, filters);
   printf("Running golden comparison function...\n");
   fflush(stdout);
   conv_golden(ifmaps, ofmaps_gold, filters);

   printf("Comparing expected vs. actual values.\n");
   fflush(stdout);

   int res = compare_expected_vs_actual(ofmaps_gold, ofmaps_synth, NUM_OUTPUTS); 
   if (res == 0) printf("Validation successful.\n");
   return res;
}
