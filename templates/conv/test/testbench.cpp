// Testbench for $lname
#include "global_defines.h"
#include "tb_utils.h"
#include "${lname}_common_defines.h"
#include <stdio.h>

void $lname (
   uram_i in_data[INPUT_RAM_SIZE],
   uram_o out_data[OUTPUT_RAM_SIZE],
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

   // Pack the inputs into URAM rows.
   uram_i ifmaps_uram[INPUT_RAM_SIZE];
   for (int i = 0; i < INPUT_RAM_SIZE; i++) {
      for (int u = 0; u < $input_words_per_uram_row; u++) {
         ifmaps_uram[i].d[u] = ifmaps[(i*$input_words_per_uram_row)+u];
      }
   }

   // Run the golden function and the synthesized function with the same inputs.
   uram_o ofmaps_synth_uram[OUTPUT_RAM_SIZE];
   data_t ofmaps_gold[NUM_OUTPUTS];
   printf("Running synthesized function...\n");
   fflush(stdout);
   $lname(ifmaps_uram, ofmaps_synth_uram, filters);
   printf("Running golden comparison function...\n");
   fflush(stdout);
   conv_golden(ifmaps, ofmaps_gold, filters);

   // Unpack the output URAM rows into one flat array for validation.
   data_t ofmaps_synth[NUM_OUTPUTS];
   for (int o = 0; o < OUTPUT_RAM_SIZE; o++) {
      for (int u = 0; u < $output_words_per_uram_row; u++) {
         ofmaps_synth[(o*$output_words_per_uram_row) + u] = ofmaps_synth_uram[o].d[u];
      }
   }

   printf("Comparing expected vs. actual values.\n");
   fflush(stdout);

   int res = compare_expected_vs_actual(ofmaps_gold, ofmaps_synth, NUM_OUTPUTS); 
   if (res == 0) printf("Validation successful.\n");
   return res;
}
