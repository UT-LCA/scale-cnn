// Accumulation stage $stage_num
// This is a "simple" accumulation stage.
// It reduces $IL inputs to 1 output.
// The estimated latency is $est_lat cycles.
data_t ${lname}_accum_${stage_num}(data_t accum_in[$IL]) {
   data_t sum = 0.0;
   for (int i = 0; i < $IL; i++) sum += accum_in[i];
   return sum;
}
