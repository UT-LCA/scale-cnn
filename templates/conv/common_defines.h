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

#endif
