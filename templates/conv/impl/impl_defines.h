#ifndef ${lname}_IMPL_H
#define ${lname}_IMPL_H

#include "../${lname}_common_defines.h"

static const int READ_SCALE_FACTOR = $read_scale_factor;
// In the actual C code, the dot_product scale factor is the same as the 
// read scale factor. We acheive doubling the dot_product scale factor by
// allowing the dot_product function to utilize both read ports on each 
// BRAM holding ifmap_vec or weight_vec data, and doubling the number of
// partial accumulators in the dot_product loop.
static const int DP_SCALE_FACTOR   = $read_scale_factor;
static const int PSUM_LEN          = $dp_scale_factor * 4;

#endif
