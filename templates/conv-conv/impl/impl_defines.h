#ifndef ${lname}_IMPL_H
#define ${lname}_IMPL_H

#include "../${lname}_common_defines.h"

static const int READ_SCALE_FACTOR  = $read_scale_factor;
static const int OCHAN_SCALE_FACTOR = $ochan_scale_factor;

// This is a hack to prevent long synthesis times during the layer synthesis step
// The synthesis time of these layers is correlated with the number of 
// iterations of the top dataflow loop. When synthesizing with the full amount 
// of iterations, the synthesis can take a very long time to complete. But with
// a much smaller number, it is very fast, typically less than a minute.
// When synthesizing entire networks, FAST_COMPILE should be 0.
#if defined __SYNTHESIS__ && FAST_COMPILE == 1
static const int TOP_LOOP_ITERATIONS = 10;
#else
static const int TOP_LOOP_ITERATIONS = NUM_L1_OUTPUTS / OCHAN_SCALE_FACTOR;
#endif

#endif
