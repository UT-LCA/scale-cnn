// This CPP file contains the function that reads data from the last layer's UltraRAMs
// and outputs it to a AXI-Stream interface.
#include "global_defines.h"
#include "hls_stream.h"
#include <inttypes.h>
#include "td_fused_axi_out.h"

static const int HEIGHT  = 14;
static const int WIDTH   = 14;
static const int CHANS   = 1000;
static const int CHANS_P = 1000;

void td_fused_axi_out (
   data_t fmaps[HEIGHT][WIDTH][CHANS_P],
   hls::stream<out_pkt> &stream_out
) {
   for (uint16_t r = 0; r < HEIGHT; r++) {
      for (uint16_t c = 0; c < WIDTH; c++) {
         for (uint16_t ch = 0; ch < CHANS_P; ch++) {
            // The target II is 4 because the inner-most loop will automatically be 
            // unrolled with a factor of 4 because of the array reshaping. The II is 
            // effectively still 1.
            #pragma HLS pipeline II=4
            out_pkt tmp;
            tmp.data = fmaps[r][c][ch];
            stream_out.write(tmp);
         }
      }
   }
}

void td_fused_axi_out_top (data_t val, hls::stream<out_pkt> &stream_out) {
   // Wrapper function to test function synthesis in isolation.
   #pragma HLS INTERFACE axis port=stream_out
   data_t fmaps[HEIGHT][WIDTH][CHANS_P];
   fmaps[0][0][0] = val;
   td_fused_axi_out(fmaps, stream_out);
}
