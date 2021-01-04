// tb_utils.cpp
// Common utilities for layer testbenches
#include "global_defines.h"

// Initialize input data to random values between 0 and 1
void gen_random_inputs(data_t *ifmaps, int len) {
   for (int i = 0; i < len; ++i) {
      ifmaps[i] = (data_t)((double)rand()/(double(RAND_MAX)));
   }
}

// Compare expected data against actual data
// Due to floating point rounding error the values might not be exactly
// the same. Since the exponents of the numbers across accumulations can 
// vary so drastically, the bounds for worst case error must be set somewhat high.
// However, as observed error increases, the number of points that have at least that 
// amount of error decreases exponentially due to the random nature of the inputs.
// 
// So I will do the data validation as follows:
//    - Make sure zero points exceed a higher error margin
//    - Make sure no more than a very small fraction of points exceed a smaller error margin.
//
// I will also compare both absolute error and percentage error to accommodate for small and large
// numbers. The difference must exceed both to be considered an error.
//
// The current bounds I am using can be seen in the constant definitions below.
int compare_expected_vs_actual(data_t *expected_data, data_t *actual_data, int num_els) {
   const float HIGH_ERROR_ABS = 0.2; 
   const float HIGH_ERROR_PCT = 0.2;
   const float LOW_ERROR_ABS  = 0.01;
   const float LOW_ERROR_PCT  = 0.01;
   int MAX_LOW_ERROR_COUNT = (num_els / 100) + 1; // Only 1% of points can exceed the low error threshold.
   int low_error_count = 0;
   for (int k = 0; k < num_els; k++) {
      float abs_error = abs(actual_data[k] - expected_data[k]);
      float pct_error = abs_error / abs(expected_data[k]);
      if (abs_error > HIGH_ERROR_ABS && pct_error > HIGH_ERROR_PCT) {
         printf("HIGH ERROR MISMATCH between expected and actual values.\n");
         printf("k=%d\n", k);
         printf("Expected = %f\n", (float)(expected_data[k]));
         printf("Actual   = %f\n", (float)(actual_data[k]));
         return -1;
      }
      if (abs_error > LOW_ERROR_ABS && pct_error > LOW_ERROR_PCT) {
         ++low_error_count;
      }
   }

   if (low_error_count > MAX_LOW_ERROR_COUNT) {
      printf("Too many points (%d out of %d) exceeded low error threshold.\n", low_error_count, num_els);
      return -1;
   }

   return 0;
}