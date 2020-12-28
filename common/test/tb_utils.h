// tb_utils.h
#pragma ONCE
#include "global_defines.h"

void gen_random_inputs(data_t *ifmaps, int len);

// Putting this in the header file because it's templated.
template <size_t len1, size_t len2>
void gen_random_filters(data_t (&filters)[len1][len2]) {
   for (int j = 0; j < len1; ++j) {
      for (int k = 0; k < len2; ++k) {
         filters[j][k] = (data_t)((double)rand()/(double(RAND_MAX/2))) - 1.0;
      }
   }
}

int compare_expected_vs_actual(data_t *expected_data, data_t *actual_data, int num_els);

