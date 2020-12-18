// Accumulation stage $stage_num
// This is a pipelined tree accumulation stage
// It reduces $IL inputs to $OL outputs.
// The estimated latency is $est_lat cycles.
data_t $${lname}_accum_${stage_num}(
   data_t accum_in[$IL], 
   data_t accum_out[$OL]
) {
   int out_idx = 0;
   IL_LOOP: for (int i = 0; i < $IL; i += $wrpc) {
      #pragma HLS pipeline
      #pragma HLS array_partition variable=vals complete
      data_t vals[$wrpc];
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (int w = 0; w < $wrpc; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < $IL) ? accum_in[i+w] : 0.0;
      }
$body
   }
}
