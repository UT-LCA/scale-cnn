// Accumulation stage $stage_num
//
// This is an interleaved accumulation stage.
// It reduces $IL inputs to $OL outputs.
// The estimated latency is $est_lat cycles.
void ${lname}_accum_${stage_num} (
   data_t accum_in[$IL],
   data_t accum_out[$OL]
) { 
   data_t psum[$OL];
   #pragma HLS array_partition variable=psum complete
   const int PSUM_LEN   = $OL;
   const int INPUT_SIZE = $IL;
   ACCUM_LOOP_OUTER: for (int x = 0; x < INPUT_SIZE; x+= PSUM_LEN) {
      #pragma HLS pipeline II=4
      ACCUM_LOOP_INNER: for (int p = 0; p < PSUM_LEN; p++) {
         // NOTE: INPUT_SIZE may not be a multiple of PSUM_LEN!
         data_t val = (x+p) < INPUT_SIZE ? (accum_in[x+p]) : (data_t)0;
         psum[p] += val;
      }
   }
   OUTPUT_LOOP: for(int q = 0; q < PSUM_LEN; q++) {
      // The outputs of this stage are stored in BRAMs.
      // This loop takes the registers and writes them into a BRAM
      // (or multiple BRAMs). Ideally there is just one BRAM but sometimes
      // we need more to prevent this stage from being the bottleneck.
      // These dependence pragmas are needed because sometimes the tools aren't
      // smart enough to realize that two writes occuring at the same are always
      // to separate addresses.
      #pragma HLS dependence variable=accum_out inter WAW false
      #pragma HLS dependence variable=accum_out intra WAW false
      #pragma HLS pipeline
      #pragma HLS unroll factor=$next_read_bw
      accum_out[q] = psum[q];
   }
}
