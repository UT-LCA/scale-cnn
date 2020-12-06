# Define the scale factor here
set SCALE_FACTOR 4

# Shared array partitioning directives
if {$SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $SCALE_FACTOR conv in_data
   set_directive_array_partition -type cyclic -factor $SCALE_FACTOR conv filter_data
   set_directive_array_partition -type cyclic -factor $SCALE_FACTOR conv ifmap_vec
   set_directive_array_partition -type cyclic -factor $SCALE_FACTOR conv weight_vec
}
set_directive_array_partition -type complete conv psum_vec
# dot_product should only use 1 port of each ifmap_vec and weight_vec BRAM
set_directive_resource -core RAM_1P_BRAM conv ifmap_vec
set_directive_resource -core RAM_1P_BRAM conv weight_vec

# readInputs directives
set_directive_pipeline readInputs/IL6
if {$SCALE_FACTOR > 1} {
   set_directive_unroll -factor $SCALE_FACTOR readInputs/IL6
}

# readFilters directives
set_directive_pipeline readFilters/FL6
if {$SCALE_FACTOR > 1} {
  set_directive_unroll -factor $SCALE_FACTOR readFilters/FL6
}

# dot_product directives
set_directive_array_partition -type complete dot_product psum

# Don't inline dot_product or accum
set_directive_inline -off dot_product
set_directive_inline -off accum

# Use dataflow optimization
set_directive_dataflow conv/TOP_LOOP

# Misc.
set_directive_array_partition -type complete conv indices


