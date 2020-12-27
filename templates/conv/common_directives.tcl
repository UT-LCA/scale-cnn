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

# Shared array partitioning directives
if {$$READ_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname filter_data
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname ifmap_vec
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname weight_vec
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname products
}

# Input array partitioning.
# The packing of words in the UltraRAMs already provides some read parallelism by itself
# So any further partitioning should only be done by a factor of read scale factor / input words per URAM row
if {$$READ_SCALE_FACTOR > $input_words_per_uram_row} {
   set_directive_array_partition -type cyclic -factor [expr {$$READ_SCALE_FACTOR / $input_words_per_uram_row}] $lname in_data
}  

# When we want dot_product to have twice the scale factor as the read functions
# (which is necessary to ensure the read functions become the bottleneck), we 
# accomplish this by allowing dot_product to utilize both read ports on the 
# ifmap_vec and weight_vec BRAMs. If we are going for a cheaper solution where
# the faster dot_product is not necessary, then we can limit it to just 1 read port
# per BRAM.
if {$$DP_SCALE_FACTOR == $$READ_SCALE_FACTOR} {
   set_directive_resource -core RAM_1P_BRAM $lname ifmap_vec
   set_directive_resource -core RAM_1P_BRAM $lname weight_vec
   set_directive_resource -core RAM_1P_BRAM $lname products
}

# Pipeline and unroll dot product multiplications
set_directive_pipeline ${lname}_dot_product/DP_LOOP
if {$$DP_SCALE_FACTOR > 1} {
   set_directive_unroll -factor $$DP_SCALE_FACTOR ${lname}_dot_product/DP_LOOP
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



