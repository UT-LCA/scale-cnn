// Read / write / dot product stages that are shared among
// conv layers and fused conv-max layers
#include "global_defines.h"
#include "${lname}_impl_defines.h"
#include <stdbool.h>
#include <assert.h>

// Reads input feature maps into an internal buffer (ifmap_vec)
// The assertions tell the compiler the upper bounds of the index
// variables which enable it to make major optimizations on the 
// arithmetic logic.
void ${lname}_readInputs ( 
   data_t in_data[INPUT_RAM_SIZE],
   int i, int j,
   data_t ifmap_vec[VECTOR_SIZE] ) {
   IL4: for (int ii = 0; ii < FILTER_SIZE; ++ii) {
      IL5: for (int jj = 0; jj < FILTER_SIZE; ++jj) {
         int row_coord = (i*STRIDE) + ii - PAD;
         int col_coord = (j*STRIDE) + jj - PAD;
         bool is_padding = (row_coord < 0) || (row_coord >= INPUT_HEIGHT) ||
                           (col_coord < 0) || (col_coord >= INPUT_WIDTH);
         int input_pixel_base = is_padding ? 0 : ((row_coord * INPUT_WIDTH) + col_coord) * INPUT_CHANS_PADDED;
         int filter_pixel_base = ((ii*FILTER_SIZE) + jj) * INPUT_CHANS;
         IL6: for (int kk = 0; kk < INPUT_CHANS; kk++) {
            int in_data_idx = input_pixel_base + kk;
            int vec_idx     = filter_pixel_base + kk;
            assert(in_data_idx < INPUT_RAM_SIZE);
            assert(vec_idx < VECTOR_SIZE);
            // Always do this read on in_data even if it's a padding pixel
            // If the read is done conditionally it causes scheduling issues.
            data_t in_data_elem = in_data[in_data_idx];
            ifmap_vec[vec_idx] = is_padding ? (data_t)0 : in_data_elem;
         }
      }
   }
}

// Reads filters into internal buffers (weight_vecs)
// It reads all the elements of OCHAN_SCALE_FACTOR filter(s)
// Like with readInputs the assertions enable compiler optimizations
void ${lname}_readFilters (
   data_t filter_data[OUTPUT_CHANS][WORDS_PER_FILTER],
   int k,
   data_t weight_vecs[OCHAN_SCALE_FACTOR][VECTOR_SIZE]
) {
   FL4: for (int ii = 0; ii < FILTER_SIZE; ++ii) {
      FL5: for (int jj = 0; jj < FILTER_SIZE; ++jj) {
         int idx_base = (ii*FILTER_SIZE + jj) * INPUT_CHANS;
         FL6: for (int kk = 0; kk < INPUT_CHANS; ++kk) {
            int idx = idx_base + kk;
            assert(idx < WORDS_PER_FILTER);
            FL7: for (int o = 0; o < OCHAN_SCALE_FACTOR; o++) {
               // This loop should always be unrolled completely.
               #pragma HLS unroll
               int ochan = (k*OCHAN_SCALE_FACTOR) + o;
               assert(ochan < OUTPUT_CHANS);
               weight_vecs[o][idx] = filter_data[ochan][idx];
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
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t out_data[OUTPUT_RAM_SIZE]
) {
   static int outputCount = 0;
   static int outputIdx   = 0;
   static data_t outputRow[$output_words_per_uram_row];
   #pragma HLS array_partition variable=outputRow complete
   for (int o = 0; o < OCHAN_SCALE_FACTOR; o++) {
      outputRow[outputCount] = outputs[o];
      outputCount++;
      if (outputCount == $output_words_per_uram_row) {
         outputCount = 0;
         for (int w = 0; w < $output_words_per_uram_row; w++) {
            #pragma HLS unroll
            out_data[(outputIdx*$output_words_per_uram_row) + w] = outputRow[w];
         }
         outputIdx++;
      }
   }
}

// This is the aligned version of writeOutputs
// In this version, we are guaranteed that OCHAN_SCALE_FACTOR is either exactly
// 4 or a multiple of 4. This enables us to avoid the use of static variables
// outputRow and outputCount and process 4 words at once in the outer loop.
void ${lname}_writeOutputs_aligned(
   data_t outputs[OCHAN_SCALE_FACTOR],
   data_t out_data[OUTPUT_RAM_SIZE]
) {
   static int row_count = 0;
   for (int o = 0; o < (OCHAN_SCALE_FACTOR/$output_words_per_uram_row); o++) {
      for (int w = 0; w < $output_words_per_uram_row; w++) {
         #pragma HLS unroll
         int outputs_idx  = (o*$output_words_per_uram_row) + w;
         int out_data_idx = (row_count*$output_words_per_uram_row) + w;
         assert(outputs_idx < OCHAN_SCALE_FACTOR);
         assert(out_data_idx < OUTPUT_RAM_SIZE);
         out_data[out_data_idx] = outputs[outputs_idx];
      }
      row_count++;
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
   data_t ifmap_vec[VECTOR_SIZE],
   data_t weight_vecs[OCHAN_SCALE_FACTOR][VECTOR_SIZE],
   data_t products[OCHAN_SCALE_FACTOR][VECTOR_SIZE]
) { 
   // Sometimes the synthesizer is unable to figure out there are no  
   // WAW dependencies for when we utilize both write ports of one BRAM.   
   // I'm not sure why this only sometimes happens and sometimes doesn't.  
   // But luckily we can explicitly tell it there are no dependencies.  
   #pragma HLS dependence variable=products inter WAW false 
   #pragma HLS dependence variable=products intra WAW false 
   DP_OUTER: for (int p = 0; p < VECTOR_SIZE; p++) {
      DP_INNER: for (int oc = 0; oc < OCHAN_SCALE_FACTOR; oc++) { 
         products[oc][p] = ifmap_vec[p] * weight_vecs[oc][p];
      }
   }
}

