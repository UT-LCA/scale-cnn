###############################################################################
#
#  ${lname}_common_directives.tcl  -- AUTO-GENERATED --
#
#  Directives used to synthesize $lname that are common regardless of
#  implementation. Some are parameterized based on implementation-specific
#  values.
#
###############################################################################

# Implement ifmaps and ofmaps as UltraRAMs
set_directive_bind_storage -type RAM_2P -impl uram ${lname} in_data
set_directive_bind_storage -type RAM_2P -impl uram ${lname} out_data

# Pack the arrays into the URAMs so we get multiple words per URAM row.
# This is achieved using array_reshape with cyclic partitioning.
# E.g. With a factor of 4, this puts elements 0,1,2,3 together in URAM row 0.
# Sometimes we may want to read even more input channels per cycle.
# To acheive this, we just need to set the partitioning factor to that number
# But we always want to pack the words into the URAMs regardless to save on memory.
# Therefore the partitioning factor should be the larger of the two numbers.
set INPUT_PART_FACTOR [expr {max($$READ_SCALE_FACTOR, $input_words_per_uram_row)}]
if {$$INPUT_PART_FACTOR > 1} {
   set_directive_array_reshape -type cyclic -factor $$INPUT_PART_FACTOR ${lname} in_data
}

# The output data reshape partitioning factor will really depend on what the next layer wants to do.
# For right now just set it to to output words per URAM row
# Eventually will need to put this elsewhere once I start synthesizing entire networks
set_directive_array_reshape -type cyclic -factor $$OCHAN_SCALE_FACTOR ${lname} out_data

# Filters / vectors / products partitioning
# Each of these arrays are two-dimensional
# filter_data dimensions are [OUTPUT_CHANS][WORDS_PER_FILTER]
# weight_vecs / products dimensions are [OCHAN_SCALE_FACTOR][WORDS_PER_FILTER]
# We want to partition all by dimensions [OCHAN_SCALE_FACTOR][READ_SCALE_FACTOR], both cyclic
# (note that the partition type for dim 1 of weight_vecs / products doesn't really matter since it is a complete partitioning)
if {$$OCHAN_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 ${lname}_top filter_data
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 $lname weight_vecs
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 $lname products
}

if {$$READ_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR        $lname ifmap_vec
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR -dim 2 $lname weight_vecs
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR -dim 2 $lname products
}

# For the filter_data, we only need to partition by half the read scale factor. This is because each BRAM has 2 read ports.
# We theoretically could do the same for weight_vecs, ifmap_vec, and products as well, but we don't because we want everything after
# the read stages to be faster than the read stages. To guarantee this, we give the dot product and first accumulation stages
# twice the read bandwidth of readFilters and readInputs.
if {$$READ_SCALE_FACTOR > 2} {
   set_directive_array_partition -type cyclic -factor [expr {$$READ_SCALE_FACTOR / 2}] -dim 2 ${lname}_top filter_data
}

# readInputs directives
# Which exact loop we pipeline and unroll depends on the scale factor, since the scale factor
# can exceed the trip count of the inner most loop.
if {$$READ_SCALE_FACTOR < $input_words_per_uram_row} {
   set unroll $$READ_SCALE_FACTOR
   set_directive_pipeline ${lname}_readInputs/IL7
   set_directive_unroll -factor $$unroll ${lname}_readInputs/IL7
} elseif {$$READ_SCALE_FACTOR < $input_chans} {
   set unroll [expr {$$READ_SCALE_FACTOR / $input_words_per_uram_row}]
   set_directive_pipeline ${lname}_readInputs/IL6
   set_directive_unroll -factor $$unroll ${lname}_readInputs/IL6
} elseif {$$READ_SCALE_FACTOR == $input_chans} {
   set_directive_pipeline ${lname}_readInputs/IL5
} else {
   # Currently do not support unrolling readInputs loops beyond a scale factor of # input channels.
   puts "Invalid read scale factor."
   exit 2
}

# readFilters directives
if {$$READ_SCALE_FACTOR < $input_chans} {
   set_directive_pipeline ${lname}_readFilters/FL6
   set_directive_unroll -factor $$READ_SCALE_FACTOR ${lname}_readFilters/FL6
} elseif {$$READ_SCALE_FACTOR == $input_chans} {
   set_directive_pipeline ${lname}_readFilters/FL5
}

# Pipeline and unroll dot product multiplications
# The unroll factor is twice the scale factor because each BRAM has two read ports.
# This doubles the number of DSPs incurred by the function but it avoids certain
# situations where dot product takes one cycle longer than the read stages.
# This one cycle can make a big impact on performance when the critical path of 
# the dataflow stages is small (sometimes as small as ~12 cycles).
set_directive_pipeline ${lname}_dot_product/DP_OUTER
set_directive_unroll -factor [expr {$$READ_SCALE_FACTOR * 2}] ${lname}_dot_product/DP_OUTER   

# Use dataflow optimization
set_directive_dataflow ${lname}/TOP_LOOP

