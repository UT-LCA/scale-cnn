###############################################################################
#
#  ${lname}_common_directives.tcl  -- AUTO-GENERATED --
#
#  Directives used to synthesize $lname that are common regardless of
#  implementation. Some are parameterized based on implementation-specific
#  values.
#
###############################################################################


# Shared array partitioning directives
if {$$READ_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname in_data
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname filter_data
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname ifmap_vec
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname weight_vec
   set_directive_array_partition -type cyclic -factor $$READ_SCALE_FACTOR $lname products
}

set_directive_array_partition -type complete $lname indices

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
if {$$READ_SCALE_FACTOR < $input_chans} {
   set_directive_pipeline ${lname}_readInputs/IL6
   set_directive_unroll -factor $$READ_SCALE_FACTOR ${lname}_readInputs/IL6
} elseif {$$READ_SCALE_FACTOR < [expr {$input_chans * $filter_size}]} {
   set_directive_pipeline ${lname}_readInputs/IL5
   set_directive_unroll -factor [expr {$$READ_SCALE_FACTOR / $input_chans}] ${lname}_readInputs/IL5
} elseif {$$READ_SCALE_FACTOR <= [expr {$input_chans * $filter_size * $filter_size}]} {
   set_directive_pipeline ${lname}_readInputs/IL4
   set_directive_unroll -factor [expr {$$READ_SCALE_FACTOR / ($input_chans * $filter_size)}] ${lname}_readInputs/IL4
}

# readFilters directives
if {$$READ_SCALE_FACTOR < $input_chans} {
   set_directive_pipeline ${lname}_readFilters/FL6
   set_directive_unroll -factor $$READ_SCALE_FACTOR ${lname}_readFilters/FL6
} elseif {$$READ_SCALE_FACTOR < [expr {$input_chans * $filter_size}]} {
   set_directive_pipeline ${lname}_readFilters/FL5
   set_directive_unroll -factor [expr {$$READ_SCALE_FACTOR / $input_chans}] ${lname}_readFilters/FL5
} elseif {$$READ_SCALE_FACTOR <= [expr {$input_chans * $filter_size * $filter_size}]} {
   set_directive_pipeline ${lname}_readFilters/FL4
   set_directive_unroll -factor [expr {$$READ_SCALE_FACTOR / ($input_chans * $filter_size)}] ${lname}_readFilters/FL4
}

# Use dataflow optimization
set_directive_dataflow ${lname}/TOP_LOOP



