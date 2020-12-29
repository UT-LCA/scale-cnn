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
set_directive_resource -core XPM_MEMORY -memory_style uram $lname in_data
set_directive_resource -core XPM_MEMORY -memory_style uram $lname out_data

# Pack the arrays into the URAMs so we get multiple words per URAM row.
set_directive_data_pack $lname in_data
set_directive_data_pack $lname out_data

# Input array partitioning.
# The packing of words in the UltraRAMs already provides some read parallelism by itself
# So any further partitioning should only be done by a factor of read scale factor / input words per URAM row
if {$$READ_SCALE_FACTOR > $input_words_per_uram_row} {
   set_directive_array_partition -type cyclic -factor [expr {$$READ_SCALE_FACTOR / $input_words_per_uram_row}] $lname in_data
}  

# ifmaps_vec partitioning
if {$$READ_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname ifmap_vec
}

# Filter / weight vectors / products partitioning
# Each of these arrays are two-dimensional
# filter_data dimensions are [OUTPUT_CHANS][WORDS_PER_FILTER]
# weight_vecs / products dimensions are [OCHAN_SCALE_FACTOR][WORDS_PER_FILTER]
# We want to partition all by dimensions [OCHAN_SCALE_FACTOR][READ_SCALE_FACTOR], both cyclic
# (note that the partition type for dim 1 of weight_vecs / products doesn't really matter since it is a complete partitioning)
# TODO: can read scale factor partitioning be divided by 2? need to experiment.
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



