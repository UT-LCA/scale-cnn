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
   const int PSUM_LEN   = $OL;
   const int INPUT_SIZE = $IL;
   INIT_LOOP: for (int i = 0; i < PSUM_LEN; i++) {
      #pragma HLS unroll
      psum[i] = 0.0;
   }
   ACCUM_LOOP_OUTER: for (int x = 0; x < INPUT_SIZE; x+= PSUM_LEN) {
      #pragma HLS pipeline
      ACCUM_LOOP_INNER: for (int p = 0; p < PSUM_LEN; p++) {
         // NOTE: INPUT_SIZE may not be a multiple of PSUM_LEN!
         data_t val = (x+p) < INPUT_SIZE ? (accum_in[x+p]) : 0.0;
         psum[p] += val;
      }
   }
   OUTPUT_LOOP: for(int q = 0; q < PSUM_LEN; q++) {
      #pragma HLS unroll
      accum_out[q] = psum[q];
   }
}
