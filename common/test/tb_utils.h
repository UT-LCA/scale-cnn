// tb_utils.h
#pragma ONCE
#include "global_defines.h"

void gen_random_inputs(data_t *ifmaps, int len);
void gen_random_filters(data_t *filters, int len);

int compare_expected_vs_actual(data_t *expected_data, data_t *actual_data, int num_els, int ichans);

