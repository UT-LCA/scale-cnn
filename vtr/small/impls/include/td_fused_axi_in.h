// Header for AXI4-Stream input layer
#include "ap_axi_sdata.h"

typedef hls::axis<data_t, 0, 0, 0> in_pkt;

void td_fused_axi_in (
   hls::stream<in_pkt> &stream_in,
   data_t fmaps[224][224][4]
);

