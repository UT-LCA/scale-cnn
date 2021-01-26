//=================================================================
//
//  -- AUTO-GENERATED --
//
//  Top-level for $name network
//
//=================================================================

#include "${name}_layers.h"
#include "hls_stream.h"

void $name(
   hls::stream<in_pkt>  &stream_in,
   hls::stream<out_pkt> &stream_out
) {
   #pragma HLS INTERFACE ap_ctrl_chain port=return

   // Feature map memories (UltraRAMs)
$fmap_declarations
   // Filter memories (Block RAMs)
$filter_declarations

   // Call each layer in sequence.
   ${name}_axi_in(stream_in, ${shorthand_name}1_fmaps);
$layer_calls
   ${name}_axi_out(final_fmaps, stream_out);

}
