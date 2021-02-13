// Accumulation stage $stage_num
// This is a pipelined tree accumulation stage
// It reduces $IL inputs to $OL outputs.
// The estimated latency is $est_lat cycles.
void ${lname}_accum_${stage_num}(
   data_t accum_in[$IL], 
   data_t accum_out[$OL]
) {
   uint16_t out_idx = 0;
   IL_LOOP: for (uint16_t i1 = 0; i1 < $trip_count; i1++) {
      uint16_t i = i1 * $wrpc;
      #pragma HLS pipeline
      data_t vals[$wrpc];
      #pragma HLS array_partition variable=vals complete
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (uint16_t w = 0; w < $wrpc; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < $IL) ? accum_in[i+w] : (data_t)0;
      }
$body
   }
}
