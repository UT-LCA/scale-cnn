// Header for AXI4-Stream input layer
#include "ap_axi_sdata.h"

typedef hls::axis<data_t, $AXIS_WUser, $AXIS_WId, $AXIS_WDest> in_pkt;

void ${name}_axi_in (
   hls::stream<in_pkt> &stream_in,
   data_t fmaps[$height][$width][$chans_padded]
);

