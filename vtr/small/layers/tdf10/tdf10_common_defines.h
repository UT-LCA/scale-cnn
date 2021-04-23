///////////////////////////////////////////////////////////////////////////////
//
// tdf10_common_defines.h  -- AUTO-GENERATED --
//
//  Includes constants that are specific to the layer and not any particular
//  implementation of the layer. This mainly consists of the layer dimensions
// 
///////////////////////////////////////////////////////////////////////////////
#ifndef tdf10_COMMON_H
#define tdf10_COMMON_H

static const int OUTPUT_HEIGHT   = 14;
static const int OUTPUT_WIDTH    = 14;
static const int OUTPUT_CHANS    = 64;
static const int L1_OUTPUT_CHANS = 512;
static const int RF_OUTPUT_CHANS = L1_OUTPUT_CHANS;

static const int FILTER_SIZE = 3;
static const int PAD         = 1;
static const int STRIDE      = 1;

static const int INPUT_HEIGHT = 14;
static const int INPUT_WIDTH  = 14;
static const int INPUT_CHANS  = 64;
static const int INPUT_CHANS_PADDED = 64;

static const int WORDS_PER_FILTER = (3 * 3 * 64);
static const int VECTOR_SIZE = WORDS_PER_FILTER;

// Constants for calculating array dimensions
// Inputs and outputs are stored in UltraRAMs while filters are stored in block RAMs.
// Each piece of data is 16 bits wide.
// BRAMs are 18 bits wide while URAMs are 72 bits wide. To maximize the utilization of 
// each URAM, we pack either 3 or 4 pieces of data in each URAM row. All elements in one
// URAM row will have the same XY coordinates but different channels. This is accomplished
// using the "ARRAY_RESHAPE" directive.
// It is assumed that INPUT_CHANS and OUTPUT_CHANS is a multiple of either 3 or 4.
//
// When INPUT_CHANS is 3, we "pad" the data with a 4th dummy channel that is unused. This
// is necessary to enable certain optimizations to be made by the synthesizer. Without this,
// the readInputs function tries to perform an unsigned division / remainder operation on the
// index, which is very expensive if divisor is 3, but free when it is 4.
static const int NUM_INPUTS      = INPUT_HEIGHT  * INPUT_WIDTH  * INPUT_CHANS;
static const int NUM_OUTPUTS     = OUTPUT_HEIGHT * OUTPUT_WIDTH * OUTPUT_CHANS;
static const int NUM_L1_OUTPUTS  = OUTPUT_HEIGHT * OUTPUT_WIDTH * L1_OUTPUT_CHANS;
static const int INPUT_RAM_SIZE  = INPUT_HEIGHT  * INPUT_WIDTH  * INPUT_CHANS_PADDED;
static const int OUTPUT_RAM_SIZE = NUM_OUTPUTS;
static const int L1_FILTER_RAM_SIZE = L1_OUTPUT_CHANS * WORDS_PER_FILTER;
static const int L2_FILTER_RAM_SIZE = L1_OUTPUT_CHANS * OUTPUT_CHANS; // *1*1 since 1x1 filters assumed.

#endif
