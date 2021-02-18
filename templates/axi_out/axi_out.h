// Header file for AXI4-Stream output layer
#include "ap_axi_sdata.h"

typedef hls::axis<data_t, $AXIS_WUser, $AXIS_WId, $AXIS_WDest> out_pkt;

void ${name}_axi_out (
   data_t fmaps[$height][$width][$chans_padded],
   hls::stream<out_pkt> &stream_out
);
