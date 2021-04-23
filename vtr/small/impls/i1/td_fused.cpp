//=================================================================
//
//  -- AUTO-GENERATED --
//
//  Top-level for td_fused network
//
//=================================================================

#include "td_fused_layers.h"
#include "hls_stream.h"
#include "td_fused_axi_in.h"

void td_fused(
   data_t tdf1_filters[16][3][3][3],
   data_t tdf2_filters[32][3][3][16],
   data_t tdf3_filters[16][1][1][32],
   data_t tdf4_filters[128][3][3][16],
   data_t tdf4_l2_filters[16][128],
   data_t tdf5_filters[128][3][3][16],
   data_t tdf6_filters[32][1][1][128],
   data_t tdf7_filters[256][3][3][32],
   data_t tdf7_l2_filters[32][256],
   data_t tdf8_filters[256][3][3][32],
   data_t tdf9_filters[64][1][1][256],
   data_t tdf10_filters[512][3][3][64],
   data_t tdf10_l2_filters[64][512],
   data_t tdf11_filters[512][3][3][64],
   data_t tdf11_l2_filters[128][512],
   data_t tdf12_filters[1000][1][1][128],

   data_t tdf1_adjustments[16][4],
   data_t tdf2_adjustments[32][4],
   data_t tdf3_adjustments[16][4],
   data_t tdf4_adjustments[128][4],
   data_t tdf4_l2_adjustments[16][4],
   data_t tdf5_adjustments[128][4],
   data_t tdf6_adjustments[32][4],
   data_t tdf7_adjustments[256][4],
   data_t tdf7_l2_adjustments[32][4],
   data_t tdf8_adjustments[256][4],
   data_t tdf9_adjustments[64][4],
   data_t tdf10_adjustments[512][4],
   data_t tdf10_l2_adjustments[64][4],
   data_t tdf11_adjustments[512][4],
   data_t tdf11_l2_adjustments[128][4],
   data_t tdf12_adjustments[1000][4],

   hls::stream<in_pkt>  &stream_in,
   hls::stream<out_pkt> &stream_out
) {
   #pragma HLS INTERFACE axis port=stream_in
   #pragma HLS INTERFACE axis port=stream_out

   // Feature map memories (UltraRAMs)
   data_t tdf1_fmaps[224][224][4];
   data_t tdf2_fmaps[112][112][16];
   data_t tdf3_fmaps[56][56][32];
   data_t tdf4_fmaps[56][56][16];
   data_t tdf5_fmaps[56][56][16];
   data_t tdf6_fmaps[28][28][128];
   data_t tdf7_fmaps[28][28][32];
   data_t tdf8_fmaps[28][28][32];
   data_t tdf9_fmaps[14][14][256];
   data_t tdf10_fmaps[14][14][64];
   data_t tdf11_fmaps[14][14][64];
   data_t tdf12_fmaps[14][14][128];
   data_t final_fmaps[14][14][1000];

   // Call each layer in sequence.
   td_fused_axi_in(stream_in, tdf1_fmaps);
   tdf1(tdf1_fmaps, tdf2_fmaps, tdf1_filters, tdf1_adjustments);
   tdf2(tdf2_fmaps, tdf3_fmaps, tdf2_filters, tdf2_adjustments);
   tdf3(tdf3_fmaps, tdf4_fmaps, tdf3_filters, tdf3_adjustments);
   tdf4(tdf4_fmaps, tdf5_fmaps, tdf4_filters, tdf4_l2_filters, tdf4_adjustments, tdf4_l2_adjustments);
   tdf5(tdf5_fmaps, tdf6_fmaps, tdf5_filters, tdf5_adjustments);
   tdf6(tdf6_fmaps, tdf7_fmaps, tdf6_filters, tdf6_adjustments);
   tdf7(tdf7_fmaps, tdf8_fmaps, tdf7_filters, tdf7_l2_filters, tdf7_adjustments, tdf7_l2_adjustments);
   tdf8(tdf8_fmaps, tdf9_fmaps, tdf8_filters, tdf8_adjustments);
   tdf9(tdf9_fmaps, tdf10_fmaps, tdf9_filters, tdf9_adjustments);
   tdf10(tdf10_fmaps, tdf11_fmaps, tdf10_filters, tdf10_l2_filters, tdf10_adjustments, tdf10_l2_adjustments);
   tdf11(tdf11_fmaps, tdf12_fmaps, tdf11_filters, tdf11_l2_filters, tdf11_adjustments, tdf11_l2_adjustments);
   tdf12(tdf12_fmaps, final_fmaps, tdf12_filters, tdf12_adjustments);

   td_fused_axi_out(final_fmaps, stream_out);

}


void td_fused_top(
   hls::stream<in_pkt>  &stream_in,
   hls::stream<out_pkt> &stream_out
) {
   // Dummy top-level wrapper. This exists just to get full resource utilization estimates.
   // Without this wrapper the BRAMs for filter data are not counted in the reports since 
   // they are external to the actual top-level function.
   #pragma HLS INTERFACE axis port=stream_in
   #pragma HLS INTERFACE axis port=stream_out

   // Filter memories (Block RAMs)
   data_t tdf1_filters[16][3][3][3];
   data_t tdf2_filters[32][3][3][16];
   data_t tdf3_filters[16][1][1][32];
   data_t tdf4_filters[128][3][3][16];
   data_t tdf4_l2_filters[16][128];
   data_t tdf5_filters[128][3][3][16];
   data_t tdf6_filters[32][1][1][128];
   data_t tdf7_filters[256][3][3][32];
   data_t tdf7_l2_filters[32][256];
   data_t tdf8_filters[256][3][3][32];
   data_t tdf9_filters[64][1][1][256];
   data_t tdf10_filters[512][3][3][64];
   data_t tdf10_l2_filters[64][512];
   data_t tdf11_filters[512][3][3][64];
   data_t tdf11_l2_filters[128][512];
   data_t tdf12_filters[1000][1][1][128];

   // Adjustment data memories (Block RAMs)
   data_t tdf1_adjustments[16][4];
   data_t tdf2_adjustments[32][4];
   data_t tdf3_adjustments[16][4];
   data_t tdf4_adjustments[128][4];
   data_t tdf4_l2_adjustments[16][4];
   data_t tdf5_adjustments[128][4];
   data_t tdf6_adjustments[32][4];
   data_t tdf7_adjustments[256][4];
   data_t tdf7_l2_adjustments[32][4];
   data_t tdf8_adjustments[256][4];
   data_t tdf9_adjustments[64][4];
   data_t tdf10_adjustments[512][4];
   data_t tdf10_l2_adjustments[64][4];
   data_t tdf11_adjustments[512][4];
   data_t tdf11_l2_adjustments[128][4];
   data_t tdf12_adjustments[1000][4];

   // Just initialize one element of each filter array to a piece of data from stream_in.
   // Of course this is not what actually will happen but this is the bare minimum to keep
   // the tools from optimizing the BRAMs out of the design.
   in_pkt tmp;
   stream_in.read_nb(tmp);
   tdf1_filters[0][0][0][0] = tmp.data;
   tdf2_filters[0][0][0][0] = tmp.data;
   tdf3_filters[0][0][0][0] = tmp.data;
   tdf4_filters[0][0][0][0] = tmp.data;
   tdf4_l2_filters[0][0] = tmp.data;
   tdf5_filters[0][0][0][0] = tmp.data;
   tdf6_filters[0][0][0][0] = tmp.data;
   tdf7_filters[0][0][0][0] = tmp.data;
   tdf7_l2_filters[0][0] = tmp.data;
   tdf8_filters[0][0][0][0] = tmp.data;
   tdf9_filters[0][0][0][0] = tmp.data;
   tdf10_filters[0][0][0][0] = tmp.data;
   tdf10_l2_filters[0][0] = tmp.data;
   tdf11_filters[0][0][0][0] = tmp.data;
   tdf11_l2_filters[0][0] = tmp.data;
   tdf12_filters[0][0][0][0] = tmp.data;

   tdf1_adjustments[0][0] = tmp.data;
   tdf2_adjustments[0][0] = tmp.data;
   tdf3_adjustments[0][0] = tmp.data;
   tdf4_adjustments[0][0] = tmp.data;
   tdf4_l2_adjustments[0][0] = tmp.data;
   tdf5_adjustments[0][0] = tmp.data;
   tdf6_adjustments[0][0] = tmp.data;
   tdf7_adjustments[0][0] = tmp.data;
   tdf7_l2_adjustments[0][0] = tmp.data;
   tdf8_adjustments[0][0] = tmp.data;
   tdf9_adjustments[0][0] = tmp.data;
   tdf10_adjustments[0][0] = tmp.data;
   tdf10_l2_adjustments[0][0] = tmp.data;
   tdf11_adjustments[0][0] = tmp.data;
   tdf11_l2_adjustments[0][0] = tmp.data;
   tdf12_adjustments[0][0] = tmp.data;

   td_fused(
      tdf1_filters,
      tdf2_filters,
      tdf3_filters,
      tdf4_filters,
      tdf4_l2_filters,
      tdf5_filters,
      tdf6_filters,
      tdf7_filters,
      tdf7_l2_filters,
      tdf8_filters,
      tdf9_filters,
      tdf10_filters,
      tdf10_l2_filters,
      tdf11_filters,
      tdf11_l2_filters,
      tdf12_filters,

      tdf1_adjustments,
      tdf2_adjustments,
      tdf3_adjustments,
      tdf4_adjustments,
      tdf4_l2_adjustments,
      tdf5_adjustments,
      tdf6_adjustments,
      tdf7_adjustments,
      tdf7_l2_adjustments,
      tdf8_adjustments,
      tdf9_adjustments,
      tdf10_adjustments,
      tdf10_l2_adjustments,
      tdf11_adjustments,
      tdf11_l2_adjustments,
      tdf12_adjustments,

      stream_in,
      stream_out
   );
   

}
