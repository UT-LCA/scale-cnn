// Header declaration for tdf7
void tdf7 (
   data_t in_data[28][28][32],
   data_t out_data[28][28][32],
   data_t l1_filter_data[256][3][3][32],
   data_t l2_filter_data[32][256],
   data_t l1_adjustments[256][4],
   data_t l2_adjustments[32][4]
);
