// Accumulation stage $stage_num
//
// This is an interleaved accumulation stage.
// It reduces $IL inputs to $OL outputs.
// The estimated latency is $est_lat cycles.
void ${lname}_accum_${stage_num} (
   data_t accum_in[$IL],
   data_t accum_out[$OL]
) { 
   data_t psum[$OL] = {0};
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
      // There is one BRAM to store the outputs and it has two write ports.
      // Therefore we can write two words per cycle.
      #pragma HLS pipeline
      #pragma HLS unroll factor=2
      accum_out[q] = psum[q];
   }
}
