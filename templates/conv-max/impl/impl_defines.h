#ifndef ${lname}_IMPL_H
#define ${lname}_IMPL_H

#include "../${lname}_common_defines.h"

static const int READ_SCALE_FACTOR  = $read_scale_factor;
static const int OCHAN_SCALE_FACTOR = $ochan_scale_factor;

// This is a temporary hack to prevent long synthesis times
// The synthesis time of these layers is correlated with the number of 
// iterations of the top dataflow loop. When synthesizing with the full amount 
// of iterations, the synthesis can take a very long time to complete. But with
// a much smaller number, it is very fast, typically less than a minute.
// Eventually I will need to remove this once everything else is working correctly.
// TODO: Remove this hack
#ifdef __SYNTHESIS__
static const int TOP_LOOP_ITERATIONS = 50 * OUTPUT_CHANS / OCHAN_SCALE_FACTOR;
#else
static const int TOP_LOOP_ITERATIONS = (NUM_OUTPUTS*POOLING_FACTOR*POOLING_FACTOR) / OCHAN_SCALE_FACTOR;
#endif

#endif
