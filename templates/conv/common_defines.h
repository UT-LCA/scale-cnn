///////////////////////////////////////////////////////////////////////////////
//
// ${lname}_common_defines.h  -- AUTO-GENERATED --
//
//  Includes constants that are specific to the layer and not any particular
//  implementation of the layer. This mainly consists of the layer dimensions
// 
///////////////////////////////////////////////////////////////////////////////
#ifndef ${lname}_COMMON_H
#define ${lname}_COMMON_H

static const int OUTPUT_HEIGHT = $output_height;
static const int OUTPUT_WIDTH  = $output_width;
static const int OUTPUT_CHANS  = $output_chans;

static const int FILTER_SIZE = $filter_size;
static const int PAD         = $pad;
static const int STRIDE      = $stride;

static const int INPUT_HEIGHT = $input_height;
static const int INPUT_WIDTH  = $input_width;
static const int INPUT_CHANS  = $input_chans;

static const int VECTOR_SIZE = ($filter_size * $filter_size * $input_chans);

// Constants for calculating array dimensions
// Inputs and outputs are stored in UltraRAMs while filters are stored in block RAMs.
// Each piece of data is 16 bits wide.
// BRAMs are 18 bits wide while URAMs are 72 bits wide. To maximize the utilization of 
// each URAM, we pack either 3 or 4 pieces of data in each URAM row. All elements in one
// URAM row will have the same XY coordinates but different channels.
// It is assumed that INPUT_CHANS and OUTPUT_CHANS is a multiple of either 3 or 4.
static const int NUM_INPUTS      = INPUT_HEIGHT  * INPUT_WIDTH  * INPUT_CHANS;
static const int NUM_OUTPUTS     = OUTPUT_HEIGHT * OUTPUT_WIDTH * OUTPUT_CHANS;
static const int INPUT_RAM_SIZE  = NUM_INPUTS / $input_words_per_uram_row;
static const int OUTPUT_RAM_SIZE = NUM_OUTPUTS / $output_words_per_uram_row;
static const int FILTER_RAM_SIZE = FILTER_SIZE * FILTER_SIZE * INPUT_CHANS * OUTPUT_CHANS;

typedef uram${input_words_per_uram_row}  uram_i;
typedef uram${output_words_per_uram_row} uram_o;

static const uram_i URAM_I_ZERO = {{0}};

#endif
