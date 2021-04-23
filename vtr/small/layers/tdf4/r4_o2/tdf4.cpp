#include "global_defines.h"
#include "tdf4_impl_defines.h"
#include <stdbool.h>
#include <assert.h>
#include "tdf4_conv_stages.h"

// Multiplies the intermediate feature maps with the second-layer
// filters. OCHAN_SCALE_FACTOR is an integer factor of the number
// of input channels. The k parameter tells us which group of L2
// input channels we're currently dealing with.
void tdf4_l2_multiply (
   data_t intermediate_fmaps[OCHAN_SCALE_FACTOR], 
   data_t l2_filter_data[OUTPUT_CHANS][L1_OUTPUT_CHANS],
   data_t l2_products[OCHAN_SCALE_FACTOR][OUTPUT_CHANS],
   uint16_t k
) {
   // Ideally this would be two separate loops but for some reason the tool isn't able to
   // flatten them. So manually flatten it in the code. The divide and modulo operations will
   // be cheap because for the conv-conv layers in Tiny Darknet, OUTPUT_CHANS is always a power of 2.
   L2_MUL: for (uint16_t i = 0; i < OUTPUT_CHANS * OCHAN_SCALE_FACTOR; i++) {
      #pragma HLS pipeline
      #pragma HLS unroll factor=2
      uint16_t l2_i = i / OUTPUT_CHANS;
      uint16_t l2_o = i % OUTPUT_CHANS;
      uint16_t l2_ichan = k*OCHAN_SCALE_FACTOR + l2_i;
      assert(l2_ichan < L1_OUTPUT_CHANS);
      assert(l2_i < OCHAN_SCALE_FACTOR);
      assert(l2_o < OUTPUT_CHANS);
      l2_products[l2_i][l2_o] = intermediate_fmaps[l2_i] * l2_filter_data[l2_o][l2_ichan];
   }
}

#if 2 > 1
// Reduces OUTPUT_CHANS groups of OCHAN_SCALE_FACTOR products into
// partial sums, one per group. This is skipped if OCHAN_SCALE_FACTOR=1
void tdf4_l2_accum (
   data_t l2_products[OCHAN_SCALE_FACTOR][OUTPUT_CHANS], 
   data_t l2_partial_sums[OUTPUT_CHANS]
) {
   L2_ACC_1: for (uint16_t group = 0; group < OUTPUT_CHANS/4; group++) {
      // Need to explicitly tell tool to not pipeline these loops
      #pragma HLS pipeline off
      data_t sums[4];
      #pragma HLS array_partition variable=sums complete
      L2_ACC_2: for (uint16_t i = 0; i < OCHAN_SCALE_FACTOR; i++) {
         #pragma HLS pipeline off
         L2_ACC_3: for (uint8_t s = 0; s < 4; s++) {
            // Unroll this inner-most loop, but do not pipeline. Running sum accumulations
            // cannot be pipelined.
            #pragma HLS pipeline off
            #pragma HLS unroll factor=4
            uint16_t out_idx = group*4 + s;
            assert(out_idx < OUTPUT_CHANS);
            sums[s] += l2_products[i][out_idx];
         }
      }
      for (uint8_t s = 0; s < 4; s++) {
         // This as well should be fully unrolled so that all writes occur in parallel.
         #pragma HLS pipeline off
         #pragma HLS unroll factor=4
         uint16_t out_idx = group*4 + s;
         assert(out_idx < OUTPUT_CHANS);
         l2_partial_sums[out_idx] = sums[s];
      }
   }
}
#endif


// Final stage in conv-conv layer pipeline.
// This stage holds an array of running sums. It receives one partial sum for each
// output channel each time it is called, pertaining to a subset of the L2 input channels.
// Once all L2 input channels have been processed, the running sums will be the final
// complete sums that can be adjusted and then written to the output URAMs. This is 
// indicated by the "write" input.
void tdf4_l2_writeOutputs (
   uint16_t i_int, uint16_t j_int, bool write,
   data_t l2_partial_sums[OUTPUT_CHANS], 
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t l2_adjustments[OUTPUT_CHANS][4]
) {
   static data_t running_sums[OUTPUT_CHANS];
   #pragma HLS bind_storage variable=running_sums type=ram_t2p impl=bram
   data_t quad[4];
   #pragma HLS array_partition variable=quad complete
   for(uint16_t ochan = 0; ochan < OUTPUT_CHANS; ochan++) {
      #pragma HLS pipeline
      #pragma HLS dependence variable=out_data inter RAW false
      data_t val = l2_partial_sums[ochan];
      data_t sum = running_sums[ochan] + val;
      // Either save the sum to accumulate a running sum, or reset to 0 when
      // we have received the final set of partial sums for these outputs.
      running_sums[ochan] = write ? (data_t)0 : sum;
      // Read the adjustments for this output channel
      data_t mean         = l2_adjustments[ochan][0];
      data_t inv_sqrt_var = l2_adjustments[ochan][1];
      data_t bias         = l2_adjustments[ochan][2];
      // Send the sum through the adjustment pipeline.
      quad[ochan % 4]     = tdf4_adjust_value(sum, mean, inv_sqrt_var, bias);
      // Every four iterations, write four values to the output all at once
      // We do it this way because the output data is stored in UltraRAMs where
      // four words are packed into a single URAM row.
      if (write && (ochan % 4 == 3)) {
         for (int q = 0; q < 4; q++) { // will be automatically unrolled
            uint16_t ochan_idx = ((ochan/4)*4) + q;
            assert(i_int < OUTPUT_HEIGHT);
            assert(j_int < OUTPUT_WIDTH);
            assert(ochan_idx < OUTPUT_CHANS);
            out_data[i_int][j_int][ochan_idx] = quad[q];
         }
      }
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
void tdf4_accum_1 (
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
void tdf4_accum_2(
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
data_t tdf4_accum_3(data_t accum_in[8]) {
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
// NOTE: For the fused conv-conv layers, OCHAN_SCALE_FACTOR pertains to the 
// "middle channels" of the feature maps between the two layers fused together.
void tdf4_get_next_ijk (uint16_t indices[3], bool *write) {
   static uint16_t i = 0;
   static uint16_t j = 0;
   static uint16_t k = 0;
   indices[0] = i;
   indices[1] = j;
   indices[2] = k;
   *write = (k == (L1_OUTPUT_CHANS / OCHAN_SCALE_FACTOR) - 1);
   k++;
   if (k == L1_OUTPUT_CHANS / OCHAN_SCALE_FACTOR) {
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


void tdf4 (
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS],
   data_t l1_filter_data[L1_OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t l2_filter_data[OUTPUT_CHANS][L1_OUTPUT_CHANS],
   data_t l1_adjustments[L1_OUTPUT_CHANS][4],
   data_t l2_adjustments[OUTPUT_CHANS][4]
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
      #pragma HLS stable variable=l1_filter_data
      #pragma HLS stable variable=l2_filter_data
      #pragma HLS stable variable=l1_adjustments
      #pragma HLS stable variable=l2_adjustments
      data_t ifmap_vec[FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
      data_t weight_vecs[OCHAN_SCALE_FACTOR][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
      data_t products[OCHAN_SCALE_FACTOR][VECTOR_SIZE];
      data_t sums[OCHAN_SCALE_FACTOR];
      data_t intermediate_fmaps[OCHAN_SCALE_FACTOR];
      #pragma HLS array_partition variable=sums complete
      #pragma HLS array_partition variable=intermediate_fmaps complete
      uint16_t indices[3];
      bool write;
      #pragma HLS array_partition variable=indices complete
      tdf4_get_next_ijk(indices, &write);
      uint16_t i_int = indices[0];
      uint16_t j_int = indices[1];
      uint16_t k_int = indices[2];
      // FOR EACH OUTPUT ELEMENT:
      //  For the L1 part of the layer:
      //  - Read the convolution window of inputs
      //  - Read the filters
      //  - Perform element-wise multiplication of the inputs and weights
      //  - Accumulate the results
      //  - Write the outputs.
      //
      //  Note that we can process multiple filters / output channels at the same time.
      //
      //  For the L2 part of the layer:
      //  - Multiply the intermediate fmaps by the L2 filter data to get OCHAN_SCALE_FACTOR * OUTPUT_CHANS products
      //  - Accumulate each group of OCHAN_SCALE_FACTOR products to get OUTPUT_CHANS partial sums
      //  - Add these partial sums to OUTPUT_CHANS running sums
      //  - After L1_OUTPUT_CHANS/OCHAN_SCALE_FACTOR iterations of accumulating the running sums, we have the final
      //    1x1xOUTPUT_CHANS data to write to the output URAMs.
      tdf4_readInputs(in_data, i_int, j_int, ifmap_vec);
      tdf4_readFilters(l1_filter_data, k_int, weight_vecs);
      tdf4_dot_product(ifmap_vec, weight_vecs, products);
      data_t accum1_out_0[32];
      data_t accum1_out_1[32];
      #pragma HLS array_partition variable=accum1_out_0 cyclic factor=2
      #pragma HLS array_partition variable=accum1_out_1 cyclic factor=2
      tdf4_accum_1(products[0], accum1_out_0);
      tdf4_accum_1(products[1], accum1_out_1);
      data_t accum2_out_0[8];
      data_t accum2_out_1[8];
      tdf4_accum_2(accum1_out_0, accum2_out_0);
      tdf4_accum_2(accum1_out_1, accum2_out_1);
      sums[0] = tdf4_accum_3(accum2_out_0);
      sums[1] = tdf4_accum_3(accum2_out_1);

      tdf4_adjust(sums, intermediate_fmaps, l1_adjustments, k_int);
      data_t l2_products[OCHAN_SCALE_FACTOR][OUTPUT_CHANS];
      #pragma HLS bind_storage variable=l2_products type=RAM_T2P impl=bram
      #pragma HLS array_partition variable=l2_products cyclic factor=2 dim=2
      tdf4_l2_multiply(intermediate_fmaps, l2_filter_data, l2_products, k_int);
      #if 2 > 1
      data_t l2_partial_sums[OUTPUT_CHANS];
      #pragma HLS bind_storage variable=l2_partial_sums type=RAM_T2P impl=bram
      #pragma HLS array_partition variable=l2_partial_sums cyclic factor=2
      tdf4_l2_accum(l2_products, l2_partial_sums);
      tdf4_l2_writeOutputs(i_int, j_int, write, l2_partial_sums, out_data, l2_adjustments);
      #else
      tdf4_l2_writeOutputs(i_int, j_int, write, l2_products[0], out_data, l2_adjustments);
      #endif
   }
}

// Top-level wrapper function for tdf4
// The output data is a port so that when we calculate cost, we don't double-count
// the UltraRAMs (since output of one layer is input to the next one).
void tdf4_top(data_t dummy_val, data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]) {
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED];
   data_t l1_filter_data[L1_OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS];
   data_t l2_filter_data[OUTPUT_CHANS][L1_OUTPUT_CHANS];
   data_t l1_adjustments[L1_OUTPUT_CHANS][4];
   data_t l2_adjustments[OUTPUT_CHANS][4];
   // Write one element to filters and adjustments to prevent tools from optimizing
   // them out. This is just to make sure the resource estimates are accurate.
   l1_filter_data[0][0][0][0] = dummy_val;
   l2_filter_data[0][0] = dummy_val;
   l1_adjustments[0][0] = dummy_val;
   l2_adjustments[0][0] = dummy_val;
   tdf4(in_data, out_data, l1_filter_data, l2_filter_data, l1_adjustments, l2_adjustments);
}

