#include "common.h"

void conv_improved (
   data_t in_data[INPUT_RAM_SIZE],
   data_t out_data[OUTPUT_RAM_SIZE],
   data_t filter_data[FILTER_RAM_SIZE]
);

int main() {
   data_t IFMAPS[INPUT_RAM_SIZE];
   data_t OFMAPS[OUTPUT_RAM_SIZE];
   data_t FILTERS[FILTER_RAM_SIZE];
   conv_improved(IFMAPS, OFMAPS, FILTERS);
   return 0;
}
