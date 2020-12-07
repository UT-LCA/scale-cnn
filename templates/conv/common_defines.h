///////////////////////////////////////////////////////////////////////////////
//
// {$lname}_common_defines.h  -- AUTO-GENERATED --
//
//  Includes constants that are specific to the layer and not any particular
//  implementation of the layer. This mainly consists of the layer dimensions
// 
///////////////////////////////////////////////////////////////////////////////
#ifndef ${lname}_COMMON_H
#define ${lname}_COMMON_H

static const int OUTPUT_HEIGHT = $OUTPUT_HEIGHT;
static const int OUTPUT_WIDTH  = $OUTPUT_WIDTH;
static const int OUTPUT_CHANS  = $OUTPUT_CHANS;

static const int FILTER_SIZE = $FILTER_SIZE;
static const int PAD         = $PAD;
static const int STRIDE      = $STRIDE;

static const int INPUT_HEIGHT = $INPUT_HEIGHT;
static const int INPUT_WIDTH  = $INPUT_WIDTH;
static const int INPUT_CHANS  = $INPUT_CHANS;

static const int VECTOR_SIZE = ($INPUT_HEIGHT * $INPUT_WIDTH * $INPUT_CHANS);
static const int INTERLEAVE  = 4; // For dot_prduct FP accumulation interleaving

#endif
