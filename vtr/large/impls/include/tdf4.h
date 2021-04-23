// Header declaration for tdf4
void tdf4 (
   data_t in_data[56][56][16],
   data_t out_data[56][56][16],
   data_t l1_filter_data[128][3][3][16],
   data_t l2_filter_data[16][128],
   data_t l1_adjustments[128][4],
   data_t l2_adjustments[16][4]
);
