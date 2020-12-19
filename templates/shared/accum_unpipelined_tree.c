// Accumulation stage $stage_num
// This is an unpipelined tree accumulation stage.
// It reduces $IL inputs to 1 output.
// The estimated latency is $est_lat cycles.
data_t ${lname}_accum_${stage_num}(data_t accum_in[$IL]) {
$body
}
