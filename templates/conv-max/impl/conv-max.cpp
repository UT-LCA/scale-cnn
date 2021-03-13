#include "global_defines.h"
#include "${lname}_impl_defines.h"
#include <stdbool.h>
#include <assert.h>

#include "${lname}_conv_stages.h"


// Pooling / writing function
// This function receives unpooled output elements and "pools" them by 
// calculating the running maximum. Once enough inputs have been gathered,
// it calls the writeOutput function with the maximum value.
void ${lname}_poolOutputs (
   uint16_t i_out, uint16_t j_out, uint16_t k, 
   bool resetMaximum, bool storeOutput, 
   data_t outputs[OCHAN_SCALE_FACTOR], 
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]
) {
   static data_t max_vals[OCHAN_SCALE_FACTOR];
   #pragma HLS array_partition variable=max_vals complete
   for (uint16_t o = 0; o < OCHAN_SCALE_FACTOR; o++) {
      #pragma HLS unroll
      bool greaterThanMaximum = (outputs[o] > max_vals[o]);
      max_vals[o] = (resetMaximum || greaterThanMaximum) ? outputs[o] : max_vals[o];
   }
   if (storeOutput) {
      ${lname}_writeOutputs_${writeFuncType}(i_out, j_out, k, max_vals, out_data);
   }
}


//////////////////////////////////////////////////////////////
//  ACCUMULATION FUNCTIONS
//////////////////////////////////////////////////////////////

$accum_functions


// Function that keeps track of indices i,j,k for the top loop
// i and j are the row and column coordinates of the unpooled outputs, respectively.
// k represents the output channel, but not directly. It actually 
// represents the group of output channels, since we can parallelize
// mutliple output channels for the same output XY coordinate. 
// For example, if OCHAN_SCALE_FACTOR = 4 (meaning we process 4 output channels
// at the same time), then k = 1 represents output channels 4,5,6,7.
//
// The order in which i,j,k change is very particular since we must account for the pooling
// that is done at the end of the dataflow pipeline. We cannot simply iterate over columns
// before moving to the next row. Instead, we must complete one "pooling window" before moving
// on to the next.
//
// For regular conv layers, we could iterate over the input coordinates as follows:
// (0,0), (0,1), (0,2), ... (0, INPUT_WIDTH-1)
//
// But if we have, for example, 2x2 pooling, we need this order:
// (0,0), (0,1), (1,0), (1,1), (0,2) ...
//
// This considerably simplifies the pooling function as otherwise it 
// would need a lot of intermediate storage to store unpooled values
// before it had all values in one pooling window.
void ${lname}_get_next_ijk ( 
   uint16_t input_indices[3],
   uint16_t output_indices[2],
   bool *resetMaximum,
   bool *storeOutput 
) {
   static uint16_t i     = 0;
   static uint16_t j     = 0;
   static uint16_t k     = 0;
   static uint16_t i_out = 0;
   static uint16_t j_out = 0;
   static uint8_t  i_p   = 0;
   static uint8_t  j_p   = 0;
   assert(i_p <= POOLING_FACTOR);
   assert(j_p <= POOLING_FACTOR);
   *resetMaximum = (i_p == 0 && j_p == 0);
   *storeOutput  = (i_p == POOLING_FACTOR-1) && (j_p == POOLING_FACTOR-1);
   input_indices[0] = i + i_p;
   input_indices[1] = j + j_p;
   input_indices[2] = k;
   output_indices[0] = i_out;
   output_indices[1] = j_out;
   j_p++;
   if (j_p == POOLING_FACTOR) {
      j_p = 0;
      i_p++;
      if (i_p == POOLING_FACTOR) {
         i_p = 0;
         k++;
         if (k == OUTPUT_CHANS / OCHAN_SCALE_FACTOR) {
            k = 0;
            j += POOLING_FACTOR;
            j_out++;
            if (j_out == OUTPUT_WIDTH) {
               j = 0;
               j_out = 0;
               i += POOLING_FACTOR;
               i_out++;
               if (i_out == OUTPUT_HEIGHT) {
                  i = 0;
                  i_out = 0;
               }
            }
         }
      }
   }
}


void $lname (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t adjustments[OUTPUT_CHANS][4]
) {
   // Ideally, this single for loop would be split into three nested loops like this,
   // where the dataflow directive would be applied to L3:
   // 
   // L1: for (int i = 0; i < OUTPUT_HEIGHT; i++) {
   //    L2: for (int j = 0; j < OUTPUT_WIDTH; j++) {
   //       L3: for (int k = 0; k < OUTPUT_CHANS / OCHAN_SCALE_FACTOR; k++) {
   //          (loop body)
   //       }
   //    }
   // }
   //
   // While this does technically work with the dataflow optimization, the synthesizer
   // is unable to properly flatten the three loops such that all calls to the dataflow
   // pipeline occur in one single contiguous stream. Instead, only (L3 trip count) calls 
   // are made in a row, and then L2 cannot begin its next iteration until the dataflow
   // pipeline is completely empty. Telling the synthesizer to explicitly flatten the loops
   // only makes the problem worse and causes the dataflow optimization to fail entirely.
   //
   // So instead, we must explicitly flatten the loops in the C code itself. The "get_next_ijk"
   // function will keep track of what the values of i,j,k would be if the loops were written 
   // as shown above.
   //
   // TODO: Figure out if this is fixed in Vitis.
   TOP_LOOP: for (int f = 0; f < TOP_LOOP_ITERATIONS; f++) {
      #pragma HLS stable variable=filter_data
      #pragma HLS stable variable=adjustments
      data_t ifmap_vec[FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
      data_t weight_vecs[OCHAN_SCALE_FACTOR][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
      data_t products[OCHAN_SCALE_FACTOR][VECTOR_SIZE];
      data_t sums[OCHAN_SCALE_FACTOR];
      data_t outputs[OCHAN_SCALE_FACTOR];
      #pragma HLS array_partition variable=sums complete
      #pragma HLS array_partition variable=outputs complete
      uint16_t input_indices[3];
      uint16_t output_indices[2];
      #pragma HLS array_partition variable=input_indices complete
      #pragma HLS array_partition variable=output_indices complete
      bool resetMaximum, storeOutput;
      ${lname}_get_next_ijk(input_indices, output_indices, &resetMaximum, &storeOutput);
      uint16_t i_in  = input_indices[0];
      uint16_t j_in  = input_indices[1];
      uint16_t k     = input_indices[2];
      uint16_t i_out = output_indices[0];
      uint16_t j_out = output_indices[1];
      // FOR EACH OUTPUT ELEMENT:
      //  - Read the convolution window of inputs
      //  - Read the filters
      //  - Perform element-wise multiplication of the inputs and weights
      //  - Accumulate the results
      //  - Adjust the sums (batch normalization, bias, activation)
      //  - Write the outputs.
      //
      //  Note that we can process multiple filters / output channels at the same time.
      ${lname}_readInputs(in_data, i_in, j_in, ifmap_vec);
      ${lname}_readFilters(filter_data, k, weight_vecs);
      ${lname}_dot_product(ifmap_vec, weight_vecs, products);
$accum_function_calls
      ${lname}_adjust(sums, outputs, adjustments, k);
      ${lname}_poolOutputs(i_out, j_out, k, resetMaximum, storeOutput, outputs, out_data);
   }
}

// Top-level wrapper function for $lname
// The output data is a port so that when we calculate cost, we don't double-count
// the UltraRAMs (since output of one layer is input to the next one).
void ${lname}_top(data_t dummy_val, data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]) {
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED];
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
   data_t adjustments[OUTPUT_CHANS][4];
   // Write one element to filters and adjustments to prevent tools from optimizing
   // them out. This is just to make sure the resource estimates are accurate.
   filter_data[0][0][0][0] = dummy_val;
   adjustments[0][0] = dummy_val;
   ${lname}(in_data, out_data, filter_data, adjustments);
}

