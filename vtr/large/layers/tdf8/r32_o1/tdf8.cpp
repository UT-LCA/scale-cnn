#include "global_defines.h"
#include "tdf8_impl_defines.h"
#include <stdbool.h>
#include <assert.h>

#include "tdf8_conv_stages.h"


// Pooling / writing function
// This function receives unpooled output elements and "pools" them by 
// calculating the running maximum. Once enough inputs have been gathered,
// it calls the writeOutput function with the maximum value.
void tdf8_poolOutputs (
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
      tdf8_writeOutputs_unaligned(i_out, j_out, k, max_vals, out_data);
   }
}


//////////////////////////////////////////////////////////////
//  ACCUMULATION FUNCTIONS
//////////////////////////////////////////////////////////////

// Accumulation stage 1
// This is a pipelined tree accumulation stage
// It reduces 288 inputs to 160 outputs.
// The estimated latency is 15 cycles.
void tdf8_accum_1(
   data_t accum_in[288], 
   data_t accum_out[160]
) {
   uint16_t out_idx = 0;
   IL_LOOP: for (uint16_t i1 = 0; i1 < 5; i1++) {
      uint16_t i = i1 * 64;
      #pragma HLS pipeline
      data_t vals[64];
      #pragma HLS array_partition variable=vals complete
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (uint16_t w = 0; w < 64; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < 288) ? accum_in[i+w] : (data_t)0;
      }
      data_t sum0 = vals[63] + vals[62];
      data_t sum1 = vals[61] + vals[60];
      data_t sum2 = vals[59] + vals[58];
      data_t sum3 = vals[57] + vals[56];
      data_t sum4 = vals[55] + vals[54];
      data_t sum5 = vals[53] + vals[52];
      data_t sum6 = vals[51] + vals[50];
      data_t sum7 = vals[49] + vals[48];
      data_t sum8 = vals[47] + vals[46];
      data_t sum9 = vals[45] + vals[44];
      data_t sum10 = vals[43] + vals[42];
      data_t sum11 = vals[41] + vals[40];
      data_t sum12 = vals[39] + vals[38];
      data_t sum13 = vals[37] + vals[36];
      data_t sum14 = vals[35] + vals[34];
      data_t sum15 = vals[33] + vals[32];
      data_t sum16 = vals[31] + vals[30];
      data_t sum17 = vals[29] + vals[28];
      data_t sum18 = vals[27] + vals[26];
      data_t sum19 = vals[25] + vals[24];
      data_t sum20 = vals[23] + vals[22];
      data_t sum21 = vals[21] + vals[20];
      data_t sum22 = vals[19] + vals[18];
      data_t sum23 = vals[17] + vals[16];
      data_t sum24 = vals[15] + vals[14];
      data_t sum25 = vals[13] + vals[12];
      data_t sum26 = vals[11] + vals[10];
      data_t sum27 = vals[9] + vals[8];
      data_t sum28 = vals[7] + vals[6];
      data_t sum29 = vals[5] + vals[4];
      data_t sum30 = vals[3] + vals[2];
      data_t sum31 = vals[1] + vals[0];
      accum_out[out_idx+0] = sum31;
      accum_out[out_idx+1] = sum30;
      accum_out[out_idx+2] = sum29;
      accum_out[out_idx+3] = sum28;
      accum_out[out_idx+4] = sum27;
      accum_out[out_idx+5] = sum26;
      accum_out[out_idx+6] = sum25;
      accum_out[out_idx+7] = sum24;
      accum_out[out_idx+8] = sum23;
      accum_out[out_idx+9] = sum22;
      accum_out[out_idx+10] = sum21;
      accum_out[out_idx+11] = sum20;
      accum_out[out_idx+12] = sum19;
      accum_out[out_idx+13] = sum18;
      accum_out[out_idx+14] = sum17;
      accum_out[out_idx+15] = sum16;
      accum_out[out_idx+16] = sum15;
      accum_out[out_idx+17] = sum14;
      accum_out[out_idx+18] = sum13;
      accum_out[out_idx+19] = sum12;
      accum_out[out_idx+20] = sum11;
      accum_out[out_idx+21] = sum10;
      accum_out[out_idx+22] = sum9;
      accum_out[out_idx+23] = sum8;
      accum_out[out_idx+24] = sum7;
      accum_out[out_idx+25] = sum6;
      accum_out[out_idx+26] = sum5;
      accum_out[out_idx+27] = sum4;
      accum_out[out_idx+28] = sum3;
      accum_out[out_idx+29] = sum2;
      accum_out[out_idx+30] = sum1;
      accum_out[out_idx+31] = sum0;
      out_idx += 32;

   }
}



// Accumulation stage 2
// This is a pipelined tree accumulation stage
// It reduces 160 inputs to 96 outputs.
// The estimated latency is 13 cycles.
void tdf8_accum_2(
   data_t accum_in[160], 
   data_t accum_out[96]
) {
   uint16_t out_idx = 0;
   IL_LOOP: for (uint16_t i1 = 0; i1 < 3; i1++) {
      uint16_t i = i1 * 64;
      #pragma HLS pipeline
      data_t vals[64];
      #pragma HLS array_partition variable=vals complete
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (uint16_t w = 0; w < 64; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < 160) ? accum_in[i+w] : (data_t)0;
      }
      data_t sum0 = vals[63] + vals[62];
      data_t sum1 = vals[61] + vals[60];
      data_t sum2 = vals[59] + vals[58];
      data_t sum3 = vals[57] + vals[56];
      data_t sum4 = vals[55] + vals[54];
      data_t sum5 = vals[53] + vals[52];
      data_t sum6 = vals[51] + vals[50];
      data_t sum7 = vals[49] + vals[48];
      data_t sum8 = vals[47] + vals[46];
      data_t sum9 = vals[45] + vals[44];
      data_t sum10 = vals[43] + vals[42];
      data_t sum11 = vals[41] + vals[40];
      data_t sum12 = vals[39] + vals[38];
      data_t sum13 = vals[37] + vals[36];
      data_t sum14 = vals[35] + vals[34];
      data_t sum15 = vals[33] + vals[32];
      data_t sum16 = vals[31] + vals[30];
      data_t sum17 = vals[29] + vals[28];
      data_t sum18 = vals[27] + vals[26];
      data_t sum19 = vals[25] + vals[24];
      data_t sum20 = vals[23] + vals[22];
      data_t sum21 = vals[21] + vals[20];
      data_t sum22 = vals[19] + vals[18];
      data_t sum23 = vals[17] + vals[16];
      data_t sum24 = vals[15] + vals[14];
      data_t sum25 = vals[13] + vals[12];
      data_t sum26 = vals[11] + vals[10];
      data_t sum27 = vals[9] + vals[8];
      data_t sum28 = vals[7] + vals[6];
      data_t sum29 = vals[5] + vals[4];
      data_t sum30 = vals[3] + vals[2];
      data_t sum31 = vals[1] + vals[0];
      accum_out[out_idx+0] = sum31;
      accum_out[out_idx+1] = sum30;
      accum_out[out_idx+2] = sum29;
      accum_out[out_idx+3] = sum28;
      accum_out[out_idx+4] = sum27;
      accum_out[out_idx+5] = sum26;
      accum_out[out_idx+6] = sum25;
      accum_out[out_idx+7] = sum24;
      accum_out[out_idx+8] = sum23;
      accum_out[out_idx+9] = sum22;
      accum_out[out_idx+10] = sum21;
      accum_out[out_idx+11] = sum20;
      accum_out[out_idx+12] = sum19;
      accum_out[out_idx+13] = sum18;
      accum_out[out_idx+14] = sum17;
      accum_out[out_idx+15] = sum16;
      accum_out[out_idx+16] = sum15;
      accum_out[out_idx+17] = sum14;
      accum_out[out_idx+18] = sum13;
      accum_out[out_idx+19] = sum12;
      accum_out[out_idx+20] = sum11;
      accum_out[out_idx+21] = sum10;
      accum_out[out_idx+22] = sum9;
      accum_out[out_idx+23] = sum8;
      accum_out[out_idx+24] = sum7;
      accum_out[out_idx+25] = sum6;
      accum_out[out_idx+26] = sum5;
      accum_out[out_idx+27] = sum4;
      accum_out[out_idx+28] = sum3;
      accum_out[out_idx+29] = sum2;
      accum_out[out_idx+30] = sum1;
      accum_out[out_idx+31] = sum0;
      out_idx += 32;

   }
}



// Accumulation stage 3
// This is a pipelined tree accumulation stage
// It reduces 96 inputs to 64 outputs.
// The estimated latency is 12 cycles.
void tdf8_accum_3(
   data_t accum_in[96], 
   data_t accum_out[64]
) {
   uint16_t out_idx = 0;
   IL_LOOP: for (uint16_t i1 = 0; i1 < 2; i1++) {
      uint16_t i = i1 * 64;
      #pragma HLS pipeline
      data_t vals[64];
      #pragma HLS array_partition variable=vals complete
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (uint16_t w = 0; w < 64; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < 96) ? accum_in[i+w] : (data_t)0;
      }
      data_t sum0 = vals[63] + vals[62];
      data_t sum1 = vals[61] + vals[60];
      data_t sum2 = vals[59] + vals[58];
      data_t sum3 = vals[57] + vals[56];
      data_t sum4 = vals[55] + vals[54];
      data_t sum5 = vals[53] + vals[52];
      data_t sum6 = vals[51] + vals[50];
      data_t sum7 = vals[49] + vals[48];
      data_t sum8 = vals[47] + vals[46];
      data_t sum9 = vals[45] + vals[44];
      data_t sum10 = vals[43] + vals[42];
      data_t sum11 = vals[41] + vals[40];
      data_t sum12 = vals[39] + vals[38];
      data_t sum13 = vals[37] + vals[36];
      data_t sum14 = vals[35] + vals[34];
      data_t sum15 = vals[33] + vals[32];
      data_t sum16 = vals[31] + vals[30];
      data_t sum17 = vals[29] + vals[28];
      data_t sum18 = vals[27] + vals[26];
      data_t sum19 = vals[25] + vals[24];
      data_t sum20 = vals[23] + vals[22];
      data_t sum21 = vals[21] + vals[20];
      data_t sum22 = vals[19] + vals[18];
      data_t sum23 = vals[17] + vals[16];
      data_t sum24 = vals[15] + vals[14];
      data_t sum25 = vals[13] + vals[12];
      data_t sum26 = vals[11] + vals[10];
      data_t sum27 = vals[9] + vals[8];
      data_t sum28 = vals[7] + vals[6];
      data_t sum29 = vals[5] + vals[4];
      data_t sum30 = vals[3] + vals[2];
      data_t sum31 = vals[1] + vals[0];
      accum_out[out_idx+0] = sum31;
      accum_out[out_idx+1] = sum30;
      accum_out[out_idx+2] = sum29;
      accum_out[out_idx+3] = sum28;
      accum_out[out_idx+4] = sum27;
      accum_out[out_idx+5] = sum26;
      accum_out[out_idx+6] = sum25;
      accum_out[out_idx+7] = sum24;
      accum_out[out_idx+8] = sum23;
      accum_out[out_idx+9] = sum22;
      accum_out[out_idx+10] = sum21;
      accum_out[out_idx+11] = sum20;
      accum_out[out_idx+12] = sum19;
      accum_out[out_idx+13] = sum18;
      accum_out[out_idx+14] = sum17;
      accum_out[out_idx+15] = sum16;
      accum_out[out_idx+16] = sum15;
      accum_out[out_idx+17] = sum14;
      accum_out[out_idx+18] = sum13;
      accum_out[out_idx+19] = sum12;
      accum_out[out_idx+20] = sum11;
      accum_out[out_idx+21] = sum10;
      accum_out[out_idx+22] = sum9;
      accum_out[out_idx+23] = sum8;
      accum_out[out_idx+24] = sum7;
      accum_out[out_idx+25] = sum6;
      accum_out[out_idx+26] = sum5;
      accum_out[out_idx+27] = sum4;
      accum_out[out_idx+28] = sum3;
      accum_out[out_idx+29] = sum2;
      accum_out[out_idx+30] = sum1;
      accum_out[out_idx+31] = sum0;
      out_idx += 32;

   }
}



// Accumulation stage 4
// This is a pipelined tree accumulation stage
// It reduces 64 inputs to 32 outputs.
// The estimated latency is 11 cycles.
void tdf8_accum_4(
   data_t accum_in[64], 
   data_t accum_out[32]
) {
   uint16_t out_idx = 0;
   IL_LOOP: for (uint16_t i1 = 0; i1 < 1; i1++) {
      uint16_t i = i1 * 64;
      #pragma HLS pipeline
      data_t vals[64];
      #pragma HLS array_partition variable=vals complete
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (uint16_t w = 0; w < 64; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < 64) ? accum_in[i+w] : (data_t)0;
      }
      data_t sum0 = vals[63] + vals[62];
      data_t sum1 = vals[61] + vals[60];
      data_t sum2 = vals[59] + vals[58];
      data_t sum3 = vals[57] + vals[56];
      data_t sum4 = vals[55] + vals[54];
      data_t sum5 = vals[53] + vals[52];
      data_t sum6 = vals[51] + vals[50];
      data_t sum7 = vals[49] + vals[48];
      data_t sum8 = vals[47] + vals[46];
      data_t sum9 = vals[45] + vals[44];
      data_t sum10 = vals[43] + vals[42];
      data_t sum11 = vals[41] + vals[40];
      data_t sum12 = vals[39] + vals[38];
      data_t sum13 = vals[37] + vals[36];
      data_t sum14 = vals[35] + vals[34];
      data_t sum15 = vals[33] + vals[32];
      data_t sum16 = vals[31] + vals[30];
      data_t sum17 = vals[29] + vals[28];
      data_t sum18 = vals[27] + vals[26];
      data_t sum19 = vals[25] + vals[24];
      data_t sum20 = vals[23] + vals[22];
      data_t sum21 = vals[21] + vals[20];
      data_t sum22 = vals[19] + vals[18];
      data_t sum23 = vals[17] + vals[16];
      data_t sum24 = vals[15] + vals[14];
      data_t sum25 = vals[13] + vals[12];
      data_t sum26 = vals[11] + vals[10];
      data_t sum27 = vals[9] + vals[8];
      data_t sum28 = vals[7] + vals[6];
      data_t sum29 = vals[5] + vals[4];
      data_t sum30 = vals[3] + vals[2];
      data_t sum31 = vals[1] + vals[0];
      accum_out[out_idx+0] = sum31;
      accum_out[out_idx+1] = sum30;
      accum_out[out_idx+2] = sum29;
      accum_out[out_idx+3] = sum28;
      accum_out[out_idx+4] = sum27;
      accum_out[out_idx+5] = sum26;
      accum_out[out_idx+6] = sum25;
      accum_out[out_idx+7] = sum24;
      accum_out[out_idx+8] = sum23;
      accum_out[out_idx+9] = sum22;
      accum_out[out_idx+10] = sum21;
      accum_out[out_idx+11] = sum20;
      accum_out[out_idx+12] = sum19;
      accum_out[out_idx+13] = sum18;
      accum_out[out_idx+14] = sum17;
      accum_out[out_idx+15] = sum16;
      accum_out[out_idx+16] = sum15;
      accum_out[out_idx+17] = sum14;
      accum_out[out_idx+18] = sum13;
      accum_out[out_idx+19] = sum12;
      accum_out[out_idx+20] = sum11;
      accum_out[out_idx+21] = sum10;
      accum_out[out_idx+22] = sum9;
      accum_out[out_idx+23] = sum8;
      accum_out[out_idx+24] = sum7;
      accum_out[out_idx+25] = sum6;
      accum_out[out_idx+26] = sum5;
      accum_out[out_idx+27] = sum4;
      accum_out[out_idx+28] = sum3;
      accum_out[out_idx+29] = sum2;
      accum_out[out_idx+30] = sum1;
      accum_out[out_idx+31] = sum0;
      out_idx += 32;

   }
}



// Accumulation stage 5
// This is a pipelined tree accumulation stage
// It reduces 32 inputs to 16 outputs.
// The estimated latency is 18 cycles.
void tdf8_accum_5(
   data_t accum_in[32], 
   data_t accum_out[16]
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
      accum_out[out_idx+0] = sum1;
      accum_out[out_idx+1] = sum0;
      out_idx += 2;

   }
}



// Accumulation stage 6
// This is a pipelined tree accumulation stage
// It reduces 16 inputs to 8 outputs.
// The estimated latency is 18 cycles.
void tdf8_accum_6(
   data_t accum_in[16], 
   data_t accum_out[8]
) {
   uint16_t out_idx = 0;
   IL_LOOP: for (uint16_t i1 = 0; i1 < 8; i1++) {
      uint16_t i = i1 * 2;
      #pragma HLS pipeline
      data_t vals[2];
      #pragma HLS array_partition variable=vals complete
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (uint16_t w = 0; w < 2; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < 16) ? accum_in[i+w] : (data_t)0;
      }
      data_t sum0 = vals[1] + vals[0];
      accum_out[out_idx+0] = sum0;
      out_idx += 1;

   }
}



// Accumulation stage 7
// This is a pipelined tree accumulation stage
// It reduces 8 inputs to 4 outputs.
// The estimated latency is 14 cycles.
void tdf8_accum_7(
   data_t accum_in[8], 
   data_t accum_out[4]
) {
   uint16_t out_idx = 0;
   IL_LOOP: for (uint16_t i1 = 0; i1 < 4; i1++) {
      uint16_t i = i1 * 2;
      #pragma HLS pipeline
      data_t vals[2];
      #pragma HLS array_partition variable=vals complete
      // This loop will be automatically unrolled and ideally all 
      // iterations of it must be scheduled in the same cycle.
      WRPC_LOOP: for (uint16_t w = 0; w < 2; w++) {
         // Need this bounds check because input length is not necessarily
         // a multiple of words read per cycle.
         vals[w] = (i+w < 8) ? accum_in[i+w] : (data_t)0;
      }
      data_t sum0 = vals[1] + vals[0];
      accum_out[out_idx+0] = sum0;
      out_idx += 1;

   }
}



// Accumulation stage 8
// This is an unpipelined tree accumulation stage.
// It reduces 4 inputs to 1 output.
// The estimated latency is 17 cycles.
data_t tdf8_accum_8(data_t accum_in[4]) {
   data_t sum0 = accum_in[3] + accum_in[2];
   data_t sum1 = accum_in[1] + accum_in[0];
   data_t sum2 = sum0 + sum1;
   return sum2;

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
void tdf8_get_next_ijk ( 
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


void tdf8 (
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
      tdf8_get_next_ijk(input_indices, output_indices, &resetMaximum, &storeOutput);
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
      tdf8_readInputs(in_data, i_in, j_in, ifmap_vec);
      tdf8_readFilters(filter_data, k, weight_vecs);
      tdf8_dot_product(ifmap_vec, weight_vecs, products);
      data_t accum1_out_0[160];
      #pragma HLS array_partition variable=accum1_out_0 cyclic factor=32
      tdf8_accum_1(products[0], accum1_out_0);
      data_t accum2_out_0[96];
      #pragma HLS array_partition variable=accum2_out_0 cyclic factor=32
      tdf8_accum_2(accum1_out_0, accum2_out_0);
      data_t accum3_out_0[64];
      #pragma HLS array_partition variable=accum3_out_0 cyclic factor=32
      tdf8_accum_3(accum2_out_0, accum3_out_0);
      data_t accum4_out_0[32];
      #pragma HLS array_partition variable=accum4_out_0 complete
      tdf8_accum_4(accum3_out_0, accum4_out_0);
      data_t accum5_out_0[16];
      #pragma HLS array_partition variable=accum5_out_0 complete
      tdf8_accum_5(accum4_out_0, accum5_out_0);
      data_t accum6_out_0[8];
      #pragma HLS array_partition variable=accum6_out_0 complete
      tdf8_accum_6(accum5_out_0, accum6_out_0);
      data_t accum7_out_0[4];
      #pragma HLS array_partition variable=accum7_out_0 complete
      tdf8_accum_7(accum6_out_0, accum7_out_0);
      sums[0] = tdf8_accum_8(accum7_out_0);

      tdf8_adjust(sums, outputs, adjustments, k);
      tdf8_poolOutputs(i_out, j_out, k, resetMaximum, storeOutput, outputs, out_data);
   }
}

// Top-level wrapper function for tdf8
// The output data is a port so that when we calculate cost, we don't double-count
// the UltraRAMs (since output of one layer is input to the next one).
void tdf8_top(data_t dummy_val, data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]) {
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED];
   data_t filter_data[OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
   data_t adjustments[OUTPUT_CHANS][4];
   // Write one element to filters and adjustments to prevent tools from optimizing
   // them out. This is just to make sure the resource estimates are accurate.
   filter_data[0][0][0][0] = dummy_val;
   adjustments[0][0] = dummy_val;
   tdf8(in_data, out_data, filter_data, adjustments);
}

