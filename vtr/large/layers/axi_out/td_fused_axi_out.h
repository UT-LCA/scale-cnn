// Header file for AXI4-Stream output layer
#include "ap_axi_sdata.h"

typedef hls::axis<data_t, 0, 0, 0> out_pkt;

void td_fused_axi_out (
   data_t fmaps[14][14][1000],
   hls::stream<out_pkt> &stream_out
);
