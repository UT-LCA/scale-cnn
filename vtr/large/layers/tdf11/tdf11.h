// Header declaration for tdf11
void tdf11 (
   data_t in_data[14][14][64],
   data_t out_data[14][14][128],
   data_t l1_filter_data[512][3][3][64],
   data_t l2_filter_data[128][512],
   data_t l1_adjustments[512][4],
   data_t l2_adjustments[128][4]
);
