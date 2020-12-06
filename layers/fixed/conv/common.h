
#ifndef COMMON_H
#define COMMON_H

#include "defines.h"

#if VIVADO_HLS == 1
#include "hls_half.h"
typedef half data_t;
#else
typedef float data_t;
#endif

#define OUTPUT_HEIGHT 56
#define OUTPUT_WIDTH  56
//#define OUTPUT_CHANS  128
// There are actually 128 output channels here
// But, for the interest of time, just do 32 for right now.
#define OUTPUT_CHANS 32

#define FILTER_SIZE 3
#define PAD         1
#define STRIDE      1

#define INPUT_HEIGHT 56
#define INPUT_WIDTH  56
#define INPUT_CHANS  16

#define INPUT_RAM_SIZE  INPUT_HEIGHT  * INPUT_WIDTH  * INPUT_CHANS
#define OUTPUT_RAM_SIZE OUTPUT_HEIGHT * OUTPUT_WIDTH * OUTPUT_CHANS
#define FILTER_RAM_SIZE FILTER_SIZE * FILTER_SIZE * INPUT_CHANS * OUTPUT_CHANS

#endif
