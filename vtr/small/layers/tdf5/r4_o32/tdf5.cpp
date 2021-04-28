#include "global_defines.h"
#include "tdf5_impl_defines.h"
#include <stdbool.h>
#include <assert.h>

#include "tdf5_conv_stages.h"


// Pooling / writing function
// This function receives unpooled output elements and "pools" them by 
// calculating the running maximum. Once enough inputs have been gathered,
// it calls the writeOutput function with the maximum value.
void tdf5_poolOutputs (
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
      tdf5_writeOutputs_aligned(i_out, j_out, k, max_vals, out_data);
   }
}


//////////////////////////////////////////////////////////////
//  ACCUMULATION FUNCTIONS
//////////////////////////////////////////////////////////////

// Accumulation stage 1
//
// This is an interleaved accumulation stage.
// It reduces 144 inputs to 32 outputs.
// The estimated latency is 33 cycles.
void tdf5_accum_1 (
   data_t accum_in[144],
   data_t accum_out[32]
) { 
   data_t psum[32];
   #pragma HLS array_partition variable=psum complete
   const int PSUM_LEN   = 32;
   const int INPUT_SIZE = 144;
   ACCUM_LOOP_OUTER: for (int x = 0; x < INPUT_SIZE; x+= PSUM_LEN) {
      #pragma HLS pipeline II=4
      ACCUM_LOOP_INNER: for (int p = 0; p < PSUM_LEN; p++) {
         // NOTE: INPUT_SIZE may not be a multiple of PSUM_LEN!
         data_t val = (x+p) < INPUT_SIZE ? (accum_in[x+p]) : (data_t)0;
         psum[p] += val;
      }
   }
   OUTPUT_LOOP: for(int q = 0; q < PSUM_LEN; q++) {
      // The outputs of this stage are stored in BRAMs.
      // This loop takes the registers and writes them into a BRAM
      // (or multiple BRAMs). Ideally there is just one BRAM but sometimes
      // we need more to prevent this stage from being the bottleneck.
      // These dependence pragmas are needed because sometimes the tools aren't
      // smart enough to realize that two writes occuring at the same are always
      // to separate addresses.
      #pragma HLS dependence variable=accum_out inter WAW false
      #pragma HLS dependence variable=accum_out intra WAW false
      #pragma HLS pipeline
      #pragma HLS unroll factor=4
      accum_out[q] = psum[q];
   }
}



// Accumulation stage 2
// This is a pipelined tree accumulation stage
// It reduces 32 inputs to 8 outputs.
// The estimated latency is 15 cycles.
void tdf5_accum_2(
   data_t accum_in[32], 
   data_t accum_out[8]
) {
   uint16_t out_idx = 0;
   IL_LOOP: for (uint16_t i1 = 0; i1 < 8; i1++) {
      uint16_t i = i1 * 4;
      #pragma HLS pipeline
      data_t vals[4];
      #pragma HLS array_partition variable=vals complete
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (uint16_t w = 0; w < 4; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < 32) ? accum_in[i+w] : (data_t)0;
      }
      data_t sum0 = vals[3] + vals[2];
      data_t sum1 = vals[1] + vals[0];
      data_t sum2 = sum0 + sum1;
      accum_out[out_idx+0] = sum2;
      out_idx += 1;

   }
}



// Accumulation stage 3
// This is a "simple" accumulation stage.
// It reduces 8 inputs to 1 output.
// The estimated latency is 25 cycles.
data_t tdf5_accum_3(data_t accum_in[8]) {
   data_t sum = 0.0;
   for (int i = 0; i < 8; i++) sum += accum_in[i];
   return sum;
}



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
void tdf5_get_next_ijk ( 
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


void tdf5 (
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
      tdf5_get_next_ijk(input_indices, output_indices, &resetMaximum, &storeOutput);
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
      tdf5_readInputs(in_data, i_in, j_in, ifmap_vec);
      tdf5_readFilters(filter_data, k, weight_vecs);
      tdf5_dot_product(ifmap_vec, weight_vecs, products);
      data_t accum1_out_0[32];
      data_t accum1_out_1[32];
      data_t accum1_out_2[32];
      data_t accum1_out_3[32];
      data_t accum1_out_4[32];
      data_t accum1_out_5[32];
      data_t accum1_out_6[32];
      data_t accum1_out_7[32];
      data_t accum1_out_8[32];
      data_t accum1_out_9[32];
      data_t accum1_out_10[32];
      data_t accum1_out_11[32];
      data_t accum1_out_12[32];
      data_t accum1_out_13[32];
      data_t accum1_out_14[32];
      data_t accum1_out_15[32];
      data_t accum1_out_16[32];
      data_t accum1_out_17[32];
      data_t accum1_out_18[32];
      data_t accum1_out_19[32];
      data_t accum1_out_20[32];
      data_t accum1_out_21[32];
      data_t accum1_out_22[32];
      data_t accum1_out_23[32];
      data_t accum1_out_24[32];
      data_t accum1_out_25[32];
      data_t accum1_out_26[32];
      data_t accum1_out_27[32];
      data_t accum1_out_28[32];
      data_t accum1_out_29[32];
      data_t accum1_out_30[32];
      data_t accum1_out_31[32];
      #pragma HLS array_partition variable=accum1_out_0 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_1 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_2 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_3 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_4 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_5 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_6 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_7 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_8 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_9 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_10 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_11 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_12 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_13 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_14 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_15 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_16 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_17 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_18 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_19 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_20 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_21 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_22 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_23 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_24 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_25 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_26 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_27 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_28 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_29 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_30 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_31 cyclic factor=2
      tdf5_accum_1(products[0], accum1_out_0);
      tdf5_accum_1(products[1], accum1_out_1);
      tdf5_accum_1(products[2], accum1_out_2);
      tdf5_accum_1(products[3], accum1_out_3);
      tdf5_accum_1(products[4], accum1_out_4);
      tdf5_accum_1(products[5], accum1_out_5);
      tdf5_accum_1(products[6], accum1_out_6);
      tdf5_accum_1(products[7], accum1_out_7);
      tdf5_accum_1(products[8], accum1_out_8);
      tdf5_accum_1(products[9], accum1_out_9);
      tdf5_accum_1(products[10], accum1_out_10);
      tdf5_accum_1(products[11], accum1_out_11);
      tdf5_accum_1(products[12], accum1_out_12);
      tdf5_accum_1(products[13], accum1_out_13);
      tdf5_accum_1(products[14], accum1_out_14);
      tdf5_accum_1(products[15], accum1_out_15);
      tdf5_accum_1(products[16], accum1_out_16);
      tdf5_accum_1(products[17], accum1_out_17);
      tdf5_accum_1(products[18], accum1_out_18);
      tdf5_accum_1(products[19], accum1_out_19);
      tdf5_accum_1(products[20], accum1_out_20);
      tdf5_accum_1(products[21], accum1_out_21);
      tdf5_accum_1(products[22], accum1_out_22);
      tdf5_accum_1(products[23], accum1_out_23);
      tdf5_accum_1(products[24], accum1_out_24);
      tdf5_accum_1(products[25], accum1_out_25);
      tdf5_accum_1(products[26], accum1_out_26);
      tdf5_accum_1(products[27], accum1_out_27);
      tdf5_accum_1(products[28], accum1_out_28);
      tdf5_accum_1(products[29], accum1_out_29);
      tdf5_accum_1(products[30], accum1_out_30);
      tdf5_accum_1(products[31], accum1_out_31);
      data_t accum2_out_0[8];
      data_t accum2_out_1[8];
      data_t accum2_out_2[8];
      data_t accum2_out_3[8];
      data_t accum2_out_4[8];
      data_t accum2_out_5[8];
      data_t accum2_out_6[8];
      data_t accum2_out_7[8];
      data_t accum2_out_8[8];
      data_t accum2_out_9[8];
      data_t accum2_out_10[8];
      data_t accum2_out_11[8];
      data_t accum2_out_12[8];
      data_t accum2_out_13[8];
      data_t accum2_out_14[8];
      data_t accum2_out_15[8];
      data_t accum2_out_16[8];
      data_t accum2_out_17[8];
      data_t accum2_out_18[8];
      data_t accum2_out_19[8];
      data_t accum2_out_20[8];
      data_t accum2_out_21[8];
      data_t accum2_out_22[8];
      data_t accum2_out_23[8];
      data_t accum2_out_24[8];
      data_t accum2_out_25[8];
      data_t accum2_out_26[8];
      data_t accum2_out_27[8];
      data_t accum2_out_28[8];
      data_t accum2_out_29[8];
      data_t accum2_out_30[8];
      data_t accum2_out_31[8];
      tdf5_accum_2(accum1_out_0, accum2_out_0);
      tdf5_accum_2(accum1_out_1, accum2_out_1);
      tdf5_accum_2(accum1_out_2, accum2_out_2);
      tdf5_accum_2(accum1_out_3, accum2_out_3);
      tdf5_accum_2(accum1_out_4, accum2_out_4);
      tdf5_accum_2(accum1_out_5, accum2_out_5);
      tdf5_accum_2(accum1_out_6, accum2_out_6);
      tdf5_accum_2(accum1_out_7, accum2_out_7);
      tdf5_accum_2(accum1_out_8, accum2_out_8);
      tdf5_accum_2(accum1_out_9, accum2_out_9);
      tdf5_accum_2(accum1_out_10, accum2_out_10);
      tdf5_accum_2(accum1_out_11, accum2_out_11);
      tdf5_accum_2(accum1_out_12, accum2_out_12);
      tdf5_accum_2(accum1_out_13, accum2_out_13);
      tdf5_accum_2(accum1_out_14, accum2_out_14);
      tdf5_accum_2(accum1_out_15, accum2_out_15);
      tdf5_accum_2(accum1_out_16, accum2_out_16);
      tdf5_accum_2(accum1_out_17, accum2_out_17);
      tdf5_accum_2(accum1_out_18, accum2_out_18);
      tdf5_accum_2(accum1_out_19, accum2_out_19);
      tdf5_accum_2(accum1_out_20, accum2_out_20);
      tdf5_accum_2(accum1_out_21, accum2_out_21);
      tdf5_accum_2(accum1_out_22, accum2_out_22);
      tdf5_accum_2(accum1_out_23, accum2_out_23);
      tdf5_accum_2(accum1_out_24, accum2_out_24);
      tdf5_accum_2(accum1_out_25, accum2_out_25);
      tdf5_accum_2(accum1_out_26, accum2_out_26);
      tdf5_accum_2(accum1_out_27, accum2_out_27);
      tdf5_accum_2(accum1_out_28, accum2_out_28);
      tdf5_accum_2(accum1_out_29, accum2_out_29);
      tdf5_accum_2(accum1_out_30, accum2_out_30);
      tdf5_accum_2(accum1_out_31, accum2_out_31);
      sums[0] = tdf5_accum_3(accum2_out_0);
      sums[1] = tdf5_accum_3(accum2_out_1);
      sums[2] = tdf5_accum_3(accum2_out_2);
      sums[3] = tdf5_accum_3(accum2_out_3);
      sums[4] = tdf5_accum_3(accum2_out_4);
      sums[5] = tdf5_accum_3(accum2_out_5);
      sums[6] = tdf5_accum_3(accum2_out_6);
      sums[7] = tdf5_accum_3(accum2_out_7);
      sums[8] = tdf5_accum_3(accum2_out_8);
      sums[9] = tdf5_accum_3(accum2_out_9);
      sums[10] = tdf5_accum_3(accum2_out_10);
      sums[11] = tdf5_accum_3(accum2_out_11);
      sums[12] = tdf5_accum_3(accum2_out_12);
      sums[13] = tdf5_accum_3(accum2_out_13);
      sums[14] = tdf5_accum_3(accum2_out_14);
      sums[15] = tdf5_accum_3(accum2_out_15);
      sums[16] = tdf5_accum_3(accum2_out_16);
      sums[17] = tdf5_accum_3(accum2_out_17);
      sums[18] = tdf5_accum_3(accum2_out_18);
      sums[19] = tdf5_accum_3(accum2_out_19);
      sums[20] = tdf5_accum_3(accum2_out_20);
      sums[21] = tdf5_accum_3(accum2_out_21);
      sums[22] = tdf5_accum_3(accum2_out_22);
      sums[23] = tdf5_accum_3(accum2_out_23);
      sums[24] = tdf5_accum_3(accum2_out_24);
      sums[25] = tdf5_accum_3(accum2_out_25);
      sums[26] = tdf5_accum_3(accum2_out_26);
      sums[27] = tdf5_accum_3(accum2_out_27);
      sums[28] = tdf5_accum_3(accum2_out_28);
      sums[29] = tdf5_accum_3(accum2_out_29);
      sums[30] = tdf5_accum_3(accum2_out_30);
      sums[31] = tdf5_accum_3(accum2_out_31);

      tdf5_adjust(sums, outputs, adjustments, k);
      tdf5_poolOutputs(i_out, j_out, k, resetMaximum, storeOutput, outputs, out_data);
   }
}

// Top-level wrapper function for tdf5
// The output data is a port so that when we calculate cost, we don't double-count
// the UltraRAMs (since output of one layer is input to the next one).
void tdf5_top(data_t dummy_val, data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]) {
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED];
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
   data_t adjustments[OUTPUT_CHANS][4];
   // Write one element to filters and adjustments to prevent tools from optimizing
   // them out. This is just to make sure the resource estimates are accurate.
   filter_data[0][0][0][0] = dummy_val;
   adjustments[0][0] = dummy_val;
   tdf5(in_data, out_data, filter_data, adjustments);
}
