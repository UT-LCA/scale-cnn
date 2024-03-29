// tb_utils.cpp
// Common utilities for layer testbenches
#include "global_defines.h"
#include <math.h>

// Initialize input data to random values between 0 and 1
void gen_random_inputs(data_t *ifmaps, int len) {
   for (int i = 0; i < len; ++i) {
      ifmaps[i] = (data_t)((double)rand()/(double(RAND_MAX)));
   }
}

// Filters: random values between -1 and 1
void gen_random_filters(data_t *filters, int len) {
   for (int i = 0; i < len; ++i) {
      filters[i] = (data_t)((double)rand()/(double(RAND_MAX/2))) - 1.0;
   }
}

// Initializes random values for the "adjustments".
// The output is a 2D array whose first dimension is len and second dimension
// is always 4.
void gen_random_adjustments(data_t adjustments[][4], int len) {
   for (int i = 0; i < len; i++) {
      // Element 0 is the mean. Pick a random value between -1 and 1
      data_t mean = (data_t)((double)rand()/(double(RAND_MAX/2))) - 1.0;
      // Element 1 is inverse square-root of variance. Pick a random value 
      // between 0.2 and 1 (since variance is always positive) 
      data_t inv_sqrt_var = (data_t)((double)rand()/(double(RAND_MAX/0.8))) + 0.2;
      // Element 2 is bias. Pick a random value betwen -2 and 2
      data_t bias = (data_t)((double)rand()/(double(RAND_MAX/4))) - 2.0;
      // Store to the output. Element 3 is unused.
      adjustments[i][0] = mean;
      adjustments[i][1] = inv_sqrt_var;
      adjustments[i][2] = bias;
      adjustments[i][3] = 0;
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
int compare_expected_vs_actual(data_t *expected_data, data_t *actual_data, int num_els, int ichans) {
   const float HIGH_ERROR_ABS = fmax(0.2, 0.04 * ichans / 16.0); 
   const float HIGH_ERROR_PCT = fmax(0.2, 0.04 * ichans / 16.0);
   // The acceptable error is higher with more and more accumulations necessary to compute one output element.
   // The number of accumulations scales with the number of input channels.
   const float LOW_ERROR_ABS  = fmax(0.01, 0.01 * ichans / 16.0);
   const float LOW_ERROR_PCT  = fmax(0.01, 0.01 * ichans / 16.0);
   int MAX_LOW_ERROR_COUNT = (num_els / 100) + 1; // Only 1% of points can exceed the low error threshold.
   int low_error_count = 0;
   int exactly_equal_count = 0;
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
      if (actual_data[k] == expected_data[k]) ++exactly_equal_count;
   }

   if (low_error_count > MAX_LOW_ERROR_COUNT) {
      printf("Too many points (%d out of %d) exceeded low error threshold.\n", low_error_count, num_els);
      return -1;
   } else {
      printf("%d out of %d points exceeded low error threshold.\n", low_error_count, num_els);
   }

   int MAX_EXACT_EQUAL_COUNT = num_els * 5 / 6;
   if (exactly_equal_count > MAX_EXACT_EQUAL_COUNT) {
      printf("Too many points (%d out of %d) equaled their exact expected values.\n", exactly_equal_count, num_els);
      return -1;
   } else {
      printf("%d out of %d points equaled their exact expected values.\n", exactly_equal_count, num_els);
   }

   return 0;
}
