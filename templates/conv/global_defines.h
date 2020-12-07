#ifndef SCALECNN_GLOBAL_DEFINES
#define SCALECNN_GLOBAL_DEFINES

// Change this to 0 if wanting to compile with gcc
#define VIVADO_HLS 1

// Use half-precision floating point data type if synthesizing with Vivado HLS,
// if compiling with gcc, just use float.
#if VIVADO_HLS == 1
#include "hls_half.h"
typedef half data_t;
#else
typedef float data_t;
#endif

// Constants for calculating array dimensions
#define INPUT_RAM_SIZE  INPUT_HEIGHT  * INPUT_WIDTH  * INPUT_CHANS
#define OUTPUT_RAM_SIZE OUTPUT_HEIGHT * OUTPUT_WIDTH * OUTPUT_CHANS
#define FILTER_RAM_SIZE FILTER_SIZE * FILTER_SIZE * INPUT_CHANS * OUTPUT_CHANS

#endif
