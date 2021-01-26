// Header for AXI4-Stream input layer
#include "ap_axi_sdata.h"

// The AXI stream data type headers only allow ap_int or ap_uint data types.
// So the data will be a ap_uint<16> which we can then just static_cast to 
// a half (data_t).
typedef hls::ap_axiu<16, $AXIS_WUser, $AXIS_WId, $AXIS_WDest> in_pkt;

void ${name}_axi_in (
   hls::stream<in_pkt> &stream_in,
   data_t fmaps[$height][$width][$chans_padded]
);

