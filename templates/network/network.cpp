//=================================================================
//
//  -- AUTO-GENERATED --
//
//  Top-level for $name network
//
//=================================================================

#include "${name}_layers.h"
#include "hls_stream.h"
#include "${name}_axi_in.h"

void $name(
$filter_data_params
$adjustment_data_params
   hls::stream<in_pkt>  &stream_in,
   hls::stream<out_pkt> &stream_out
) {
   #pragma HLS INTERFACE axis port=stream_in
   #pragma HLS INTERFACE axis port=stream_out

   // Feature map memories (UltraRAMs)
$fmap_declarations
   // Call each layer in sequence.
   ${name}_axi_in(stream_in, ${shorthand_name}1_fmaps);
$layer_calls
   ${name}_axi_out(final_fmaps, stream_out);

}


void ${name}_top(
   hls::stream<in_pkt>  &stream_in,
   hls::stream<out_pkt> &stream_out
) {
   // Dummy top-level wrapper. This exists just to get full resource utilization estimates.
   // Without this wrapper the BRAMs for filter data are not counted in the reports since 
   // they are external to the actual top-level function.
   #pragma HLS INTERFACE axis port=stream_in
   #pragma HLS INTERFACE axis port=stream_out

   // Filter memories (Block RAMs)
$filter_declarations
   // Adjustment data memories (Block RAMs)
$adjustment_declarations
   // Just initialize one element of each filter array to a piece of data from stream_in.
   // Of course this is not what actually will happen but this is the bare minimum to keep
   // the tools from optimizing the BRAMs out of the design.
   in_pkt tmp;
   stream_in.read_nb(tmp);
$filter_init
$adjustment_init
   ${name}(
$filter_data_params_top
$adjustment_data_params_top
      stream_in,
      stream_out
   );
   

}
