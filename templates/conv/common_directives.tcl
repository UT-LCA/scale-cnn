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
if {$$SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $$SCALE_FACTOR $lname in_data
   set_directive_array_partition -type cyclic -factor $$SCALE_FACTOR $lname filter_data
   set_directive_array_partition -type cyclic -factor $$SCALE_FACTOR $lname ifmap_vec
   set_directive_array_partition -type cyclic -factor $$SCALE_FACTOR $lname weight_vec
}

set_directive_array_partition -type complete ${lname}_dot_product psum
set_directive_array_partition -type complete $lname psum_vec
set_directive_array_partition -type complete $lname indices

# dot_product should only use 1 port of each ifmap_vec and weight_vec BRAM
# These might be unnecessary? Need to investigate
set_directive_resource -core RAM_1P_BRAM $lname ifmap_vec
set_directive_resource -core RAM_1P_BRAM $lname weight_vec

# readInputs directives
if {$$SCALE_FACTOR < $input_chans} {
   set_directive_pipeline ${lname}_readInputs/IL6
   set_directive_unroll -factor $$SCALE_FACTOR ${lname}_readInputs/IL6
} elseif {$$SCALE_FACTOR < [expr {$input_chans * $filter_size}]} {
   set_directive_pipeline ${lname}_readInputs/IL5
   set_directive_unroll -factor [expr {$$SCALE_FACTOR / $input_chans}] ${lname}_readInputs/IL5
} elseif {$$SCALE_FACTOR <= [expr {$input_chans * $filter_size * $filter_size}]} {
   set_directive_pipeline ${lname}_readInputs/IL4
   set_directive_unroll -factor [expr {$$SCALE_FACTOR / ($input_chans * $filter_size)}] ${lname}_readInputs/IL4
}

# readFilters directives
if {$$SCALE_FACTOR < $input_chans} {
   set_directive_pipeline ${lname}_readFilters/FL6
   set_directive_unroll -factor $$SCALE_FACTOR ${lname}_readFilters/FL6
} elseif {$$SCALE_FACTOR < [expr {$input_chans * $filter_size}]} {
   set_directive_pipeline ${lname}_readFilters/FL5
   set_directive_unroll -factor [expr {$$SCALE_FACTOR / $input_chans}] ${lname}_readFilters/FL5
} elseif {$$SCALE_FACTOR <= [expr {$input_chans * $filter_size * $filter_size}]} {
   set_directive_pipeline ${lname}_readFilters/FL4
   set_directive_unroll -factor [expr {$$SCALE_FACTOR / ($input_chans * $filter_size)}] ${lname}_readFilters/FL4
}


# Don't inline dot_product or accum
set_directive_inline -off ${lname}_dot_product
set_directive_inline -off ${lname}_accum

# Use dataflow optimization
set_directive_dataflow ${lname}/TOP_LOOP



