#include "global_defines.h"
#include "tdf9_impl_defines.h"
#include <stdbool.h>
#include <assert.h>

#include "tdf9_conv_stages.h"

//////////////////////////////////////////////////////////////
//  ACCUMULATION FUNCTIONS
//////////////////////////////////////////////////////////////

// Accumulation stage 1
//
// This is an interleaved accumulation stage.
// It reduces 256 inputs to 8 outputs.
// The estimated latency is 137 cycles.
void tdf9_accum_1 (
   data_t accum_in[256],
   data_t accum_out[8]
) { 
   data_t psum[8];
   #pragma HLS array_partition variable=psum complete
   const int PSUM_LEN   = 8;
   const int INPUT_SIZE = 256;
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
      #pragma HLS unroll factor=2
      accum_out[q] = psum[q];
   }
}



// Accumulation stage 2
// This is a "simple" accumulation stage.
// It reduces 8 inputs to 1 output.
// The estimated latency is 25 cycles.
data_t tdf9_accum_2(data_t accum_in[8]) {
   data_t sum = 0.0;
   for (int i = 0; i < 8; i++) sum += accum_in[i];
   return sum;
}



// Function that keeps track of indices i,j,k for the top loop
// i and j are the output row and column coordinates, respectively
// k represents the output channel, but not directly. It actually 
// represents the group of output channels, since we can parallelize
// mutliple output channels for the same output XY coordinate. 
// For example, if OCHAN_SCALE_FACTOR = 4 (meaning we process 4 output channels
// at the same time), then k = 1 represents output channels 4,5,6,7.
void tdf9_get_next_ijk (uint16_t indices[3]) {
   static uint16_t i = 0;
   static uint16_t j = 0;
   static uint16_t k = 0;
   indices[0] = i;
   indices[1] = j;
   indices[2] = k;
   k++;
   if (k == OUTPUT_CHANS / OCHAN_SCALE_FACTOR) {
      k = 0;
      j++;
      if (j == OUTPUT_WIDTH) {
         j = 0;
         i++;
         if (i == OUTPUT_HEIGHT) {
            i = 0;
         }
      }
   }
}


void tdf9 (
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
      uint16_t indices[3];
      #pragma HLS array_partition variable=indices complete
      tdf9_get_next_ijk(indices);
      uint16_t i_int = indices[0];
      uint16_t j_int = indices[1];
      uint16_t k_int = indices[2];
      // FOR EACH OUTPUT ELEMENT:
      //  - Read the convolution window of inputs
      //  - Read the filters
      //  - Perform element-wise multiplication of the inputs and weights
      //  - Accumulate the results
      //  - Adjust the sums (batch normalization, bias, activation)
      //  - Write the outputs.
      //
      //  Note that we can process multiple filters / output channels at the same time.
      tdf9_readInputs(in_data, i_int, j_int, ifmap_vec);
      tdf9_readFilters(filter_data, k_int, weight_vecs);
      tdf9_dot_product(ifmap_vec, weight_vecs, products);
      data_t accum1_out_0[8];
      data_t accum1_out_1[8];
      data_t accum1_out_2[8];
      data_t accum1_out_3[8];
      data_t accum1_out_4[8];
      data_t accum1_out_5[8];
      data_t accum1_out_6[8];
      data_t accum1_out_7[8];
      data_t accum1_out_8[8];
      data_t accum1_out_9[8];
      data_t accum1_out_10[8];
      data_t accum1_out_11[8];
      data_t accum1_out_12[8];
      data_t accum1_out_13[8];
      data_t accum1_out_14[8];
      data_t accum1_out_15[8];
      data_t accum1_out_16[8];
      data_t accum1_out_17[8];
      data_t accum1_out_18[8];
      data_t accum1_out_19[8];
      data_t accum1_out_20[8];
      data_t accum1_out_21[8];
      data_t accum1_out_22[8];
      data_t accum1_out_23[8];
      data_t accum1_out_24[8];
      data_t accum1_out_25[8];
      data_t accum1_out_26[8];
      data_t accum1_out_27[8];
      data_t accum1_out_28[8];
      data_t accum1_out_29[8];
      data_t accum1_out_30[8];
      data_t accum1_out_31[8];
      data_t accum1_out_32[8];
      data_t accum1_out_33[8];
      data_t accum1_out_34[8];
      data_t accum1_out_35[8];
      data_t accum1_out_36[8];
      data_t accum1_out_37[8];
      data_t accum1_out_38[8];
      data_t accum1_out_39[8];
      data_t accum1_out_40[8];
      data_t accum1_out_41[8];
      data_t accum1_out_42[8];
      data_t accum1_out_43[8];
      data_t accum1_out_44[8];
      data_t accum1_out_45[8];
      data_t accum1_out_46[8];
      data_t accum1_out_47[8];
      data_t accum1_out_48[8];
      data_t accum1_out_49[8];
      data_t accum1_out_50[8];
      data_t accum1_out_51[8];
      data_t accum1_out_52[8];
      data_t accum1_out_53[8];
      data_t accum1_out_54[8];
      data_t accum1_out_55[8];
      data_t accum1_out_56[8];
      data_t accum1_out_57[8];
      data_t accum1_out_58[8];
      data_t accum1_out_59[8];
      data_t accum1_out_60[8];
      data_t accum1_out_61[8];
      data_t accum1_out_62[8];
      data_t accum1_out_63[8];
      tdf9_accum_1(products[0], accum1_out_0);
      tdf9_accum_1(products[1], accum1_out_1);
      tdf9_accum_1(products[2], accum1_out_2);
      tdf9_accum_1(products[3], accum1_out_3);
      tdf9_accum_1(products[4], accum1_out_4);
      tdf9_accum_1(products[5], accum1_out_5);
      tdf9_accum_1(products[6], accum1_out_6);
      tdf9_accum_1(products[7], accum1_out_7);
      tdf9_accum_1(products[8], accum1_out_8);
      tdf9_accum_1(products[9], accum1_out_9);
      tdf9_accum_1(products[10], accum1_out_10);
      tdf9_accum_1(products[11], accum1_out_11);
      tdf9_accum_1(products[12], accum1_out_12);
      tdf9_accum_1(products[13], accum1_out_13);
      tdf9_accum_1(products[14], accum1_out_14);
      tdf9_accum_1(products[15], accum1_out_15);
      tdf9_accum_1(products[16], accum1_out_16);
      tdf9_accum_1(products[17], accum1_out_17);
      tdf9_accum_1(products[18], accum1_out_18);
      tdf9_accum_1(products[19], accum1_out_19);
      tdf9_accum_1(products[20], accum1_out_20);
      tdf9_accum_1(products[21], accum1_out_21);
      tdf9_accum_1(products[22], accum1_out_22);
      tdf9_accum_1(products[23], accum1_out_23);
      tdf9_accum_1(products[24], accum1_out_24);
      tdf9_accum_1(products[25], accum1_out_25);
      tdf9_accum_1(products[26], accum1_out_26);
      tdf9_accum_1(products[27], accum1_out_27);
      tdf9_accum_1(products[28], accum1_out_28);
      tdf9_accum_1(products[29], accum1_out_29);
      tdf9_accum_1(products[30], accum1_out_30);
      tdf9_accum_1(products[31], accum1_out_31);
      tdf9_accum_1(products[32], accum1_out_32);
      tdf9_accum_1(products[33], accum1_out_33);
      tdf9_accum_1(products[34], accum1_out_34);
      tdf9_accum_1(products[35], accum1_out_35);
      tdf9_accum_1(products[36], accum1_out_36);
      tdf9_accum_1(products[37], accum1_out_37);
      tdf9_accum_1(products[38], accum1_out_38);
      tdf9_accum_1(products[39], accum1_out_39);
      tdf9_accum_1(products[40], accum1_out_40);
      tdf9_accum_1(products[41], accum1_out_41);
      tdf9_accum_1(products[42], accum1_out_42);
      tdf9_accum_1(products[43], accum1_out_43);
      tdf9_accum_1(products[44], accum1_out_44);
      tdf9_accum_1(products[45], accum1_out_45);
      tdf9_accum_1(products[46], accum1_out_46);
      tdf9_accum_1(products[47], accum1_out_47);
      tdf9_accum_1(products[48], accum1_out_48);
      tdf9_accum_1(products[49], accum1_out_49);
      tdf9_accum_1(products[50], accum1_out_50);
      tdf9_accum_1(products[51], accum1_out_51);
      tdf9_accum_1(products[52], accum1_out_52);
      tdf9_accum_1(products[53], accum1_out_53);
      tdf9_accum_1(products[54], accum1_out_54);
      tdf9_accum_1(products[55], accum1_out_55);
      tdf9_accum_1(products[56], accum1_out_56);
      tdf9_accum_1(products[57], accum1_out_57);
      tdf9_accum_1(products[58], accum1_out_58);
      tdf9_accum_1(products[59], accum1_out_59);
      tdf9_accum_1(products[60], accum1_out_60);
      tdf9_accum_1(products[61], accum1_out_61);
      tdf9_accum_1(products[62], accum1_out_62);
      tdf9_accum_1(products[63], accum1_out_63);
      sums[0] = tdf9_accum_2(accum1_out_0);
      sums[1] = tdf9_accum_2(accum1_out_1);
      sums[2] = tdf9_accum_2(accum1_out_2);
      sums[3] = tdf9_accum_2(accum1_out_3);
      sums[4] = tdf9_accum_2(accum1_out_4);
      sums[5] = tdf9_accum_2(accum1_out_5);
      sums[6] = tdf9_accum_2(accum1_out_6);
      sums[7] = tdf9_accum_2(accum1_out_7);
      sums[8] = tdf9_accum_2(accum1_out_8);
      sums[9] = tdf9_accum_2(accum1_out_9);
      sums[10] = tdf9_accum_2(accum1_out_10);
      sums[11] = tdf9_accum_2(accum1_out_11);
      sums[12] = tdf9_accum_2(accum1_out_12);
      sums[13] = tdf9_accum_2(accum1_out_13);
      sums[14] = tdf9_accum_2(accum1_out_14);
      sums[15] = tdf9_accum_2(accum1_out_15);
      sums[16] = tdf9_accum_2(accum1_out_16);
      sums[17] = tdf9_accum_2(accum1_out_17);
      sums[18] = tdf9_accum_2(accum1_out_18);
      sums[19] = tdf9_accum_2(accum1_out_19);
      sums[20] = tdf9_accum_2(accum1_out_20);
      sums[21] = tdf9_accum_2(accum1_out_21);
      sums[22] = tdf9_accum_2(accum1_out_22);
      sums[23] = tdf9_accum_2(accum1_out_23);
      sums[24] = tdf9_accum_2(accum1_out_24);
      sums[25] = tdf9_accum_2(accum1_out_25);
      sums[26] = tdf9_accum_2(accum1_out_26);
      sums[27] = tdf9_accum_2(accum1_out_27);
      sums[28] = tdf9_accum_2(accum1_out_28);
      sums[29] = tdf9_accum_2(accum1_out_29);
      sums[30] = tdf9_accum_2(accum1_out_30);
      sums[31] = tdf9_accum_2(accum1_out_31);
      sums[32] = tdf9_accum_2(accum1_out_32);
      sums[33] = tdf9_accum_2(accum1_out_33);
      sums[34] = tdf9_accum_2(accum1_out_34);
      sums[35] = tdf9_accum_2(accum1_out_35);
      sums[36] = tdf9_accum_2(accum1_out_36);
      sums[37] = tdf9_accum_2(accum1_out_37);
      sums[38] = tdf9_accum_2(accum1_out_38);
      sums[39] = tdf9_accum_2(accum1_out_39);
      sums[40] = tdf9_accum_2(accum1_out_40);
      sums[41] = tdf9_accum_2(accum1_out_41);
      sums[42] = tdf9_accum_2(accum1_out_42);
      sums[43] = tdf9_accum_2(accum1_out_43);
      sums[44] = tdf9_accum_2(accum1_out_44);
      sums[45] = tdf9_accum_2(accum1_out_45);
      sums[46] = tdf9_accum_2(accum1_out_46);
      sums[47] = tdf9_accum_2(accum1_out_47);
      sums[48] = tdf9_accum_2(accum1_out_48);
      sums[49] = tdf9_accum_2(accum1_out_49);
      sums[50] = tdf9_accum_2(accum1_out_50);
      sums[51] = tdf9_accum_2(accum1_out_51);
      sums[52] = tdf9_accum_2(accum1_out_52);
      sums[53] = tdf9_accum_2(accum1_out_53);
      sums[54] = tdf9_accum_2(accum1_out_54);
      sums[55] = tdf9_accum_2(accum1_out_55);
      sums[56] = tdf9_accum_2(accum1_out_56);
      sums[57] = tdf9_accum_2(accum1_out_57);
      sums[58] = tdf9_accum_2(accum1_out_58);
      sums[59] = tdf9_accum_2(accum1_out_59);
      sums[60] = tdf9_accum_2(accum1_out_60);
      sums[61] = tdf9_accum_2(accum1_out_61);
      sums[62] = tdf9_accum_2(accum1_out_62);
      sums[63] = tdf9_accum_2(accum1_out_63);

      tdf9_adjust(sums, outputs, adjustments, k_int);
      tdf9_writeOutputs_aligned(i_int, j_int, k_int, outputs, out_data);
   }
}

// Top-level wrapper function for tdf9
// The output data is a port so that when we calculate cost, we don't double-count
// the UltraRAMs (since output of one layer is input to the next one).
void tdf9_top(data_t dummy_val, data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]) {
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED];
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
   data_t adjustments[OUTPUT_CHANS][4];
   // Write one element to filters and adjustments to prevent tools from optimizing
   // them out. This is just to make sure the resource estimates are accurate.
   filter_data[0][0][0][0] = dummy_val;
   adjustments[0][0] = dummy_val;
   tdf9(in_data, out_data, filter_data, adjustments);
}
