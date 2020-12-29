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
set_directive_bind_storage -type RAM_2P -impl uram $lname in_data
set_directive_bind_storage -type RAM_2P -impl uram $lname out_data

# Pack the arrays into the URAMs so we get multiple words per URAM row.
# This is achieved using array_reshape with cyclic partitioning.
# E.g. With a factor of 4, this puts elements 0,1,2,3 together in URAM row 0.
# Sometimes we may want to read even more input channels per cycle.
# To acheive this, we just need to set the partitioning factor to that number
# But we always want to pack the words into the URAMs regardless to save on memory.
# Therefore the partitioning factor should be the larger of the two numbers.
set INPUT_PART_FACTOR [expr {max($$READ_SCALE_FACTOR, $input_words_per_uram_row)}]
if {$$INPUT_PART_FACTOR > 1} {
   set_directive_array_reshape -type cyclic -factor $$INPUT_PART_FACTOR $lname in_data
}

# The output data reshape partitioning factor will really depend on what the next layer wants to do.
# For right now just set it to to output words per URAM row
# Eventually will need to put this elsewhere once I start synthesizing entire networks
set_directive_array_reshape -type cyclic -factor $$OCHAN_SCALE_FACTOR $lname out_data

# ifmap_vec partitioning
if {$$READ_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname ifmap_vec
}

# Filter / weight vectors / products partitioning
# Each of these arrays are two-dimensional
# filter_data dimensions are [OUTPUT_CHANS][WORDS_PER_FILTER]
# weight_vecs / products dimensions are [OCHAN_SCALE_FACTOR][WORDS_PER_FILTER]
# We want to partition all by dimensions [OCHAN_SCALE_FACTOR][READ_SCALE_FACTOR], both cyclic
# (note that the partition type for dim 1 of weight_vecs / products doesn't really matter since it is a complete partitioning)
# TODO: can read scale factor partitioning be divided by 2 due to the 2 read ports on BRAMs? need to experiment.
if {$$OCHAN_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 $lname filter_data
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 $lname weight_vecs
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 $lname products
}

if {$$READ_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR  -dim 2 $lname filter_data
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR  -dim 2 $lname weight_vecs
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR  -dim 2 $lname products
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

# Use dataflow optimization
set_directive_dataflow ${lname}/TOP_LOOP



