// This CPP file contains the function that reads in data from an AXI4-Stream
// interface and writes it to UltraRAMs holding the ifmaps for the first layer.
#include "global_defines.h"
#include "hls_stream.h"
#include <inttypes.h>
#include "${name}_axi_in.h"

static const int HEIGHT  = $height;
static const int WIDTH   = $width;
static const int CHANS   = $chans;
static const int CHANS_P = $chans_padded;

void ${name}_axi_in (
   hls::stream<in_pkt> &stream_in,
   data_t fmaps[HEIGHT][WIDTH][CHANS_P]
) {
   for (uint16_t r = 0; r < HEIGHT; r++) {
      for (uint16_t c = 0; c < WIDTH; c++) {
         data_t p[CHANS_P] = {0};
         for (uint16_t ch = 0; ch < CHANS; ch++) {
            #pragma HLS pipeline
            in_pkt tmp;
            stream_in.read_nb(tmp);
            p[ch] = static_cast<data_t>(tmp.data);
            if (ch == CHANS-1) {
               for (uint16_t ch_p = 0; ch_p < CHANS_P; ch_p++) {
                  fmaps[r][c][ch_p] = p[ch_p];
               }
            }
         }
      }
   }
}

data_t ${name}_axi_in_top (hls::stream<in_pkt> &stream_in) {
   // Wrapper function to test function synthesis in isolation.
   #pragma HLS INTERFACE axis port=stream_in
   data_t fmaps[HEIGHT][WIDTH][CHANS_P];
   ${name}_axi_in(stream_in, fmaps);
   return fmaps[0][0][0];
}
