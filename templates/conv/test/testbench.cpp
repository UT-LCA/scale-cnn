// Testbench for $lname
#include "global_defines.h"
#include "tb_utils.h"
#include "${lname}_common_defines.h"
#include <stdio.h>

void $lname (
   data_t in_data[INPUT_RAM_SIZE],
   data_t out_data[OUTPUT_RAM_SIZE],
   data_t filter_data[FILTER_RAM_SIZE]
);

void conv_golden (
   data_t in_data[INPUT_RAM_SIZE],
   data_t out_data[OUTPUT_RAM_SIZE],
   data_t filter_data[FILTER_RAM_SIZE]
);

int main() {

   printf("Beginning functional validation.\n");
   printf("Generating random inputs.\n");
   fflush(stdout);

   // Generate random inputs and filters
   data_t ifmaps[INPUT_RAM_SIZE];
   data_t filters[FILTER_RAM_SIZE];
   gen_random_inputs(ifmaps, INPUT_RAM_SIZE);
   gen_random_filters(filters, FILTER_RAM_SIZE);

   // Run the golden function and the synthesized function with the same inputs.
   data_t ofmaps_synth[OUTPUT_RAM_SIZE];
   data_t ofmaps_gold[OUTPUT_RAM_SIZE];
   printf("Running synthesized function...\n");
   fflush(stdout);
   $lname(ifmaps, ofmaps_synth, filters);
   printf("Running golden comparison function...\n");
   fflush(stdout);
   conv_golden(ifmaps, ofmaps_gold, filters);

   printf("Comparing expected vs. actual values.\n");
   fflush(stdout);

   int res = compare_expected_vs_actual(ofmaps_gold, ofmaps_synth, OUTPUT_RAM_SIZE); 
   if (res == 0) printf("Validation successful.\n");
   return res;
}
