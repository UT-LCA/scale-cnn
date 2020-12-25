// Testbench for $lname
#include "global_defines.h"
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
   fflush(stdout);

   // Create random inputs.
   data_t ifmaps[INPUT_RAM_SIZE];
   for (int i = 0; i < INPUT_RAM_SIZE; i++) {
      // Generate random values between 0 and 1
      ifmaps[i] = (half)((double)rand()/(double(RAND_MAX)));
   }

   // Create random filters.
   // Generate random values between -1 and +1
   data_t filters[FILTER_RAM_SIZE];
   for (int j = 0; j < FILTER_RAM_SIZE; j++) {
      filters[j] = (half)((double)rand()/(double(RAND_MAX/2))) - 1.0;
   }

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

   // Make sure they are the same.
   for (int k = 0; k < OUTPUT_RAM_SIZE; k++) {
      // Due to floating point rounding error the values might not be exactly
      // the same. Make sure they are either within 0.01 or 1% within each other
      float abs_error = abs(ofmaps_synth[k] - ofmaps_gold[k]);
      float pct_error = abs_error / abs(ofmaps_gold[k]);
      if (abs_error > 0.01 && pct_error > 0.01) {
         printf("MISMATCH between expected and actual values.\n");
         printf("k=%d\n", k);
         printf("Expected = %f\n", (float)(ofmaps_gold[k]));
         printf("Actual   = %f\n", (float)(ofmaps_synth[k]));
         return -1;
      }
   }
   
   printf("Validation successful.\n");
   return 0;

}
