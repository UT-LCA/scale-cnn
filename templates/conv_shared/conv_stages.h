// Read / write / dot product stages that are shared among
// conv layers and fused conv-max layers
#include "global_defines.h"
#include "${lname}_impl_defines.h"
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>

// Reads input feature maps into an internal buffer (ifmap_vec)
// The assertions tell the compiler the upper bounds of the index
// variables which enable it to make major optimizations on the 
// arithmetic logic.
void ${lname}_readInputs ( 
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
void ${lname}_readFilters (
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
void ${lname}_writeOutputs_unaligned(
   uint16_t i, uint16_t j, uint16_t k,
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]
) {
   #pragma HLS inline off
   static uint16_t outputCount   = 0;
   static uint16_t outputChanIdx = 0;
   static data_t outputRow[$output_words_per_uram_row];
   #pragma HLS array_partition variable=outputRow complete
   for (uint16_t o = 0; o < OCHAN_SCALE_FACTOR; o++) {
      outputRow[outputCount] = outputs[o];
      outputCount++;
      if (outputCount == $output_words_per_uram_row) {
         outputCount = 0;
         for (uint16_t w = 0; w < $output_words_per_uram_row; w++) {
            #pragma HLS unroll
            uint16_t ochan = (outputChanIdx*$output_words_per_uram_row) + w;
            assert(ochan < OUTPUT_CHANS);
            out_data[i][j][ochan] = outputRow[w];
         }
         outputChanIdx++;
         if (outputChanIdx*$output_words_per_uram_row == OUTPUT_CHANS) {
            outputChanIdx = 0;
         }
      }
   }
}


// This is the aligned version of writeOutputs
// In this version, we are guaranteed that OCHAN_SCALE_FACTOR is either exactly
// 4 or a multiple of 4. This enables us to avoid the use of static variables
// outputRow and outputCount and process 4 words at once in the outer loop.
void ${lname}_writeOutputs_aligned(
   uint16_t i, uint16_t j, uint16_t k,
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t out_data[OUTPUT_HEIGHT][OUTPUT_WIDTH][OUTPUT_CHANS]
) {
   #pragma HLS inline off
   static const uint16_t OUTER_ITERS = (OCHAN_SCALE_FACTOR/$output_words_per_uram_row);
   for (uint16_t o = 0; o < OUTER_ITERS; o++) {
      for (uint16_t w = 0; w < $output_words_per_uram_row; w++) {
         #pragma HLS unroll
         uint16_t out_data_ochan = w + $output_words_per_uram_row * (o + OUTER_ITERS * k);
         uint16_t outputs_ochan  = (o*$output_words_per_uram_row) + w;
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
void ${lname}_dot_product (
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

