// Read / write / dot product stages that are shared among
// conv layers and fused conv-max layers
#include "global_defines.h"
#include "tdf2_impl_defines.h"
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>
#include "hls_math.h"

// Reads input feature maps into an internal buffer (ifmap_vec)
// The assertions tell the compiler the upper bounds of the index
// variables which enable it to make major optimizations on the 
// arithmetic logic.
void tdf2_readInputs ( 
   data_t in_data[INPUT_HEIGHT][INPUT_WIDTH][INPUT_CHANS_PADDED],
   uint16_t i, uint16_t j,
   data_t ifmap_vec[FILTER_SIZE][FILTER_SIZE][INPUT_CHANS] 
) {
   IL4: for (int ii = 0; ii < FILTER_SIZE; ++ii) {
      IL5: for (int jj = 0; jj < FILTER_SIZE; ++jj) {
         int row_coord = ((int)i*STRIDE) + ii - PAD;
         int col_coord = ((int)j*STRIDE) + jj - PAD;
         bool is_padding = (row_coord < 0) || (row_coord >= INPUT_HEIGHT) ||
                           (col_coord < 0) || (col_coord >= INPUT_WIDTH);
         IL6: for (uint16_t kk = 0; kk < INPUT_CHANS; kk++) {
            int row_coord_int = is_padding ? 0 : row_coord;
            int col_coord_int = is_padding ? 0 : col_coord;
            assert(row_coord_int < INPUT_HEIGHT);
            assert(col_coord_int < INPUT_WIDTH);
            // Always do this read on in_data even if it's a padding pixel
            // If the read is done conditionally it causes scheduling issues.
            data_t in_data_elem = in_data[row_coord_int][col_coord_int][kk];
            ifmap_vec[ii][jj][kk] = is_padding ? (data_t)0 : in_data_elem;
         }
      }
   }
}


// Reads filters into internal buffers (weight_vecs)
// It reads all the elements of OCHAN_SCALE_FACTOR filter(s)
void tdf2_readFilters (
   data_t filter_data[RF_OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   uint16_t k,
   data_t weight_vecs[OCHAN_SCALE_FACTOR][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS]
) {
   FL4: for (uint16_t ii = 0; ii < FILTER_SIZE; ++ii) {
      FL5: for (uint16_t jj = 0; jj < FILTER_SIZE; ++jj) {
         FL6: for (uint16_t kk = 0; kk < INPUT_CHANS; ++kk) {
            FL7: for (uint16_t o = 0; o < OCHAN_SCALE_FACTOR; o++) {
               // This loop should always be unrolled completely.
               #pragma HLS unroll
               uint16_t ochan = k*OCHAN_SCALE_FACTOR + o;
               assert(ochan < RF_OUTPUT_CHANS);
               weight_vecs[o][ii][jj][kk] = filter_data[ochan][ii][jj][kk];
            }
         }
      }
   }
}  


// Function for writing output data to the output URAMs.
// This function receives OCHAN_SCALE_FACTOR words per function call, and packs 
// the words together before writing them to the output URAM. Each URAM row will 
// hold 4 elements. In this version of the function, OCHAN_SCALE_FACTOR is not a
// multiple of 4 (and may be less than 4). For this reason, this function must use 
// static variables to store partial URAM rows so that it only writes to the URAM 
// when an entire row is ready. If OCHAN_SCALE_FACTOR is less than 4, then not every 
// call to this function will result in an actual write to the URAMs.
//
// Due to the nature of how static variables are accessed, the outer loop cannot be
// pipelined. This is not a big deal because in most cases, OCHAN_SCALE_FACTOR will
// be either 1 or 2, so pipelining would not even make sense.
void tdf2_writeOutputs_unaligned(
   uint16_t i, uint16_t j, uint16_t k,
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]
) {
   #pragma HLS inline off
   static uint16_t outputCount   = 0;
   static uint16_t outputChanIdx = 0;
   static data_t outputRow[4];
   #pragma HLS array_partition variable=outputRow complete
   for (uint16_t o = 0; o < OCHAN_SCALE_FACTOR; o++) {
      outputRow[outputCount] = outputs[o];
      outputCount++;
      if (outputCount == 4) {
         outputCount = 0;
         for (uint16_t w = 0; w < 4; w++) {
            #pragma HLS unroll
            uint16_t ochan = (outputChanIdx*4) + w;
            assert(ochan < OUTPUT_CHANS);
            out_data[i][j][ochan] = outputRow[w];
         }
         outputChanIdx++;
         if (outputChanIdx*4 == OUTPUT_CHANS) {
            outputChanIdx = 0;
         }
      }
   }
}


// This is the aligned version of writeOutputs
// In this version, we are guaranteed that OCHAN_SCALE_FACTOR is either exactly
// 4 or a multiple of 4. This enables us to avoid the use of static variables
// outputRow and outputCount and process 4 words at once in the outer loop.
void tdf2_writeOutputs_aligned(
   uint16_t i, uint16_t j, uint16_t k,
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]
) {
   #pragma HLS inline off
   static const uint16_t OUTER_ITERS = (OCHAN_SCALE_FACTOR/4);
   for (uint16_t o = 0; o < OUTER_ITERS; o++) {
      for (uint16_t w = 0; w < 4; w++) {
         #pragma HLS unroll
         uint16_t out_data_ochan = w + 4 * (o + OUTER_ITERS * k);
         uint16_t outputs_ochan  = (o*4) + w;
         assert(out_data_ochan < OUTPUT_CHANS);
         assert(outputs_ochan < OCHAN_SCALE_FACTOR);
         out_data[i][j][out_data_ochan] = outputs[outputs_ochan];
      }
   }
}


// Calculates the dot product of the ifmap and weight vectors.
// All this stage does is multiply the numbers together into an array of 
// products. The accumulation is handled by separate stages.
// This stage also handles multiplying the inputs by different filters
// at the same time. This needs to be handled in the inner loop of the 
// function so that the synthesizer can take advantage of only reading 
// each input data once.
void tdf2_dot_product (
   data_t ifmap_vec[FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t weight_vecs[OCHAN_SCALE_FACTOR][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS],
   data_t products[OCHAN_SCALE_FACTOR][VECTOR_SIZE]
) { 
   // Sometimes the synthesizer is unable to figure out there are no  
   // WAW dependencies for when we utilize both write ports of one BRAM.   
   // I'm not sure why this only sometimes happens and sometimes doesn't.  
   // But luckily we can explicitly tell it there are no dependencies.  
   #pragma HLS dependence variable=products inter WAW false 
   #pragma HLS dependence variable=products intra WAW false 
   DP_OUTER_1: for (uint16_t ii = 0; ii < FILTER_SIZE; ii++) {
      DP_OUTER_2: for (uint16_t jj = 0; jj < FILTER_SIZE; jj++) {
         DP_OUTER_3: for (uint16_t ic = 0; ic < INPUT_CHANS; ic++) {
            uint16_t p = ic + INPUT_CHANS * (jj + FILTER_SIZE*ii);
            assert(p < VECTOR_SIZE);
            DP_INNER: for (uint16_t oc = 0; oc < OCHAN_SCALE_FACTOR; oc++) { 
               products[oc][p] = ifmap_vec[ii][jj][ic] * weight_vecs[oc][ii][jj][ic];
            }
         }
      }
   }
}


// Performs the "adjustment" after accumulation. This handles batch normalization, bias,
// and activation. Currently, the only supported activation is ReLU, as it is the simplest
// and cheapest to implement an FPGA.
//
// Batch normalization mean/variance and bias are calculated during training and are loaded 
// into the FPGA in the same manner as filter data. Each accumulation result is passed through
// this function to first noramlize the data, then add the bias, and finally pass the result
// through the activation function. This gives the final output element for a layer.
//
// There is one mean, variance, and bias for each filter in a layer. The values will be stored in
// BRAMs. Rather than storing the variance directly, we instead store the pre-computed 1/sqrt(variance)
// to simplify the work that needs to be done on chip.
//
// If it is desired to apply a "scale" to multiply to the normalized value before adding
// a bias, this can be accomplished simply by multiplying inverse square-root variance
// with the desired scale value during pre-computation outside the FPGA.
//
// This function is intentionally inlined and is intended to be pipelined by the caller.
data_t tdf2_adjust_value (
   data_t val_in,
   data_t mean,
   data_t inv_sqrt_variance,
   data_t bias ) 
{
   #pragma HLS inline
   data_t normalized = (val_in - mean) * inv_sqrt_variance;
   data_t biased     = normalized + bias;
   // Rather than doing a comparison to 0, just check the sign bit directly so that
   // the tools do not infer an entire hcmp component.
   bool negative = (bool)(hls::signbit(biased));
   data_t activated  = negative ? (data_t)0 : biased; // ReLU activation
   return activated;
}


// Adjustment stage for CONV layers. This handles batch normalization, bias, and activation.
// See above comments for more details on the adjustment.
// 
// The function takes in the sums that are the outputs of the accumulation stages and makes
// all adjustments to calculate the final layer outputs.
//
// "adjustments" holds all values required to do the adjustments -- mainly, mean and variance
// (actually 1/sqrt(variance)), and bias. While this is just three words, the dimension is 4
// because the tools cannot gracefully handle array partitioning with non-power-of-2 factors.
//
// The integer "k" represents the group of output channels being processed at once.
void tdf2_adjust (
   data_t sums[OCHAN_SCALE_FACTOR],
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t adjustments[RF_OUTPUT_CHANS][4],
   uint16_t k)
{
   ADJUST_LOOP: for (uint16_t o = 0; o < OCHAN_SCALE_FACTOR; o++) {
      #pragma HLS pipeline
      // This loop may be unrolled by the CONV directives. However it will 
      // only be unrolled by factor 2 for simplicity. Any need to unroll it 
      // beyond that to acheive an II of 1 can only realistically occur for 
      // layers with very high scale factors (unlikely to be used).
      uint16_t ochan = k*OCHAN_SCALE_FACTOR + o;
      data_t mean         = adjustments[ochan][0];
      data_t inv_sqrt_var = adjustments[ochan][1];
      data_t bias         = adjustments[ochan][2];
      outputs[o] = tdf2_adjust_value(sums[o], mean, inv_sqrt_var, bias);
   }
}
