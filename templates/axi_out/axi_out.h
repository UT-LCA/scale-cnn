// Header file for AXI4-Stream output layer
#include "ap_axi_sdata.h"

// The AXI stream data type headers only allow ap_int or ap_uint data types.
// So the data will be a ap_uint<16> which we can then just static_cast from
// a half (data_t).
typedef hls::ap_axiu<16, $AXIS_WUser, $AXIS_WId, $AXIS_WDest> out_pkt;

void ${name}_axi_out (
   data_t fmaps[$height][$width][$chans_padded],
   hls::stream<out_pkt> &stream_out
);
