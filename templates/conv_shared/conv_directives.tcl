###############################################################################
#
#  ${lname}_conv_directives.tcl  -- AUTO-GENERATED --
#
#  Directives used to synthesize $lname that are common regardless of
#  implementation. Some are parameterized based on implementation-specific
#  values. These directives are shared between conv and conv-max stages.
#
###############################################################################

# Implement ifmaps and ofmaps as UltraRAMs
# RAM_S2P means 2-port RAM where one only does write operations and the other
# only does read operations.
set_directive_bind_storage -type RAM_S2P -impl uram $$top $$in_data
if {$$final_layer} {
   set_directive_bind_storage -type RAM_S2P -impl uram $$top $$out_data
}

# Implement filter data as Block RAMs.
set_directive_bind_storage -type RAM_2P -impl bram $$top $$filter_data

# Pack the input data into the URAMs so we get multiple words per URAM row.
# This is achieved using array_reshape with cyclic partitioning.
# E.g. With a factor of 4, this puts elements 0,1,2,3 together in URAM row 0.
# Sometimes we may want to read even more input channels per cycle.
# To acheive this, we just need to set the partitioning factor to that number
# But we always want to pack the words into the URAMs regardless to save on memory.
# Therefore the reshape partitioning factor should always be at least 4
set INPUT_RESHAPE_FACTOR [expr {max($$READ_SCALE_FACTOR, $input_words_per_uram_row)}]
set_directive_array_reshape -type cyclic -factor $$INPUT_RESHAPE_FACTOR $$top $$in_data -dim 3

# The output data reshape partitioning factor will really depend on what the next layer wants to do.
# For right now just set it to to output words per URAM row
if {$$final_layer} {
   set_directive_array_reshape -type cyclic -factor $output_words_per_uram_row $$top $$out_data -dim 3
}

# Filters / vectors / products partitioning
# ifmap_vec dimensions are [FILTER_SIZE][FILTER_SIZE][INPUT_CHANS]
# filter_data dimensions are [OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS]
# weight_vecs dimensions are [OCHAN_SCALE_FACTOR][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS]
# products dimensions are [OCHAN_SCALE_FACTOR][FILTER_SIZE*FILTER_SIZE*INPUT_CHANS]
# We want to partition the output channel dimension by OCHAN_SCALE_FACTOR
# And the input channel dimension by READ_SCALE_FACTOR
# (note that the partition type for dim 1 of weight_vecs / products doesn't really matter since it is a complete partitioning)
if {$$OCHAN_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 $$top  $$filter_data
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 $lname weight_vecs
   set_directive_array_partition -type cyclic -factor $$OCHAN_SCALE_FACTOR -dim 1 $lname products
}

if {$$READ_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR -dim 3 $lname ifmap_vec
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR -dim 4 $lname weight_vecs
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR -dim 2 $lname products
}

# For the filter_data, we only need to partition by half the read scale factor. This is because each BRAM has 2 read ports.
# We theoretically could do the same for weight_vecs, ifmap_vec, and products as well, but we don't because we want everything after
# the read stages to be faster than the read stages. To guarantee this, we give the dot product and first accumulation stages
# twice the read bandwidth of readFilters and readInputs.
# Read scale factor could be odd so need to round up.
if {$$READ_SCALE_FACTOR > 2} {
   set_directive_array_partition -type cyclic -factor [expr {int(ceil($$READ_SCALE_FACTOR / 2.0))}] -dim 4 $$top $$filter_data
}

# readInputs directives
# Which exact loop we pipeline and unroll depends on the scale factor, since the scale factor
# can exceed the trip count of the inner most loop.
if {$$READ_SCALE_FACTOR < $input_chans} {
   set unroll $$READ_SCALE_FACTOR
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
if {$$READ_SCALE_FACTOR < $input_chans} {
   set_directive_pipeline ${lname}_dot_product/DP_OUTER_3
   set_directive_unroll -factor $$READ_SCALE_FACTOR ${lname}_dot_product/DP_OUTER_3
} elseif {$$READ_SCALE_FACTOR == $input_chans} {
   set_directive_pipeline ${lname}_dot_product/DP_OUTER_2
}

# Use dataflow optimization
set_directive_dataflow ${lname}/TOP_LOOP

