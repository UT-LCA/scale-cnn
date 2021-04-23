###############################################################################
#
#  tdf4_conv_directives.tcl  -- AUTO-GENERATED --
#
#  Directives used to synthesize tdf4 that are common regardless of
#  implementation. Some are parameterized based on implementation-specific
#  values. These directives are shared between conv and conv-max stages.
#
###############################################################################

# Implement ifmaps and ofmaps as UltraRAMs
# RAM_S2P means 2-port RAM where one only does write operations and the other
# only does read operations.
set_directive_bind_storage -type ram_s2p -impl uram $top $in_data
if {$final_layer} {
   set_directive_bind_storage -type ram_s2p -impl uram $top $out_data
}

# Implement filter data with either Block RAMs or UltraRAMs. The tool will decide which.
# For conv-conv layers, L2 filter data will always be in Block RAMs because it is typically
# less data.
set filter_ram_type bram
set_directive_bind_storage -type ram_2p -impl $filter_ram_type $filter_top $filter_data
if {$layer_type == "conv-conv"} {
   set_directive_bind_storage -type ram_2p -impl bram $filter_top $filter_data_2
}

# Implement adjustment data with brams.
set_directive_bind_storage -type ram_2p -impl bram $filter_top $adjustments
if {$layer_type == "conv-conv"} {
   set_directive_bind_storage -type ram_2p -impl bram $filter_top $adjustments_2
}
   

# Pack the input data into the URAMs so we get multiple words per URAM row.
# This is achieved using array_reshape with cyclic partitioning.
# E.g. With a factor of 4, this puts elements 0,1,2,3 together in URAM row 0.
# Sometimes we may want to read even more input channels per cycle.
# To acheive this, we just need to set the partitioning factor to that number
# But we always want to pack the words into the URAMs regardless to save on memory.
# Therefore the reshape partitioning factor should always be at least 4
# Reshape filters with the same factor as the inputs.
set INPUT_RESHAPE_FACTOR [expr {max($READ_SCALE_FACTOR, 4)}]
set_directive_array_reshape -type cyclic -factor $INPUT_RESHAPE_FACTOR $top $in_data -dim 3
# If filters are in BRAMs, we do not need to reshape beyond the read scale factor
# But if they are in URAMs, we need to reshape with minimum factor 4 for the same reason as the inputs.
if {$filter_ram_type == "uram"} {
   set filter_reshape_factor $INPUT_RESHAPE_FACTOR
} else {
   set filter_reshape_factor $READ_SCALE_FACTOR
}
set_directive_array_reshape -type cyclic -factor $filter_reshape_factor $filter_top $filter_data -dim 4

# The output data reshape partitioning factor will really depend on what the next layer wants to do,
# since the outputs are the inputs to the next layer. So only set this when this is the final layer.
if {$final_layer} {
   set_directive_array_reshape -type cyclic -factor 4 $top $out_data -dim 3
}

# Reshape dimension 2 of adjustments so that we can read all adjustment values for one channel simultaneously.
# This might not be necessary for layers with small output channel scaling factor. Not doing it means the 
# adjustment pipeline cannot acheive an II of 1, but this might not matter. In the future this could be explored
# to save resources for such layers. But just always reshape for now to keep things simple.
set_directive_array_reshape -type cyclic -factor 4 $filter_top $adjustments -dim 2
if {$layer_type == "conv-conv"} {
   set_directive_array_reshape -type cyclic -factor 4 $filter_top $adjustments_2 -dim 2
}

# Filters / vectors / products partitioning
# ifmap_vec dimensions are [FILTER_SIZE][FILTER_SIZE][INPUT_CHANS]
# filter_data dimensions are [OUTPUT_CHANS][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS]
# weight_vecs dimensions are [OCHAN_SCALE_FACTOR][FILTER_SIZE][FILTER_SIZE][INPUT_CHANS]
# products dimensions are [OCHAN_SCALE_FACTOR][FILTER_SIZE*FILTER_SIZE*INPUT_CHANS]
# We want to partition the output channel dimension by OCHAN_SCALE_FACTOR
# And the input channel dimension by READ_SCALE_FACTOR
# (note that the partition type for dim 1 of weight_vecs / products doesn't really matter since it is a complete partitioning)
if {$OCHAN_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $OCHAN_SCALE_FACTOR -dim 1 $filter_top $filter_data
   set_directive_array_partition -type cyclic -factor $OCHAN_SCALE_FACTOR -dim 1 tdf4 weight_vecs
   set_directive_array_partition -type cyclic -factor $OCHAN_SCALE_FACTOR -dim 1 tdf4 products
}

if {$READ_SCALE_FACTOR > 1} {
   set_directive_array_partition -type cyclic -factor $READ_SCALE_FACTOR -dim 3 tdf4 ifmap_vec
   set_directive_array_partition -type cyclic -factor $READ_SCALE_FACTOR -dim 4 tdf4 weight_vecs
   set_directive_array_partition -type cyclic -factor $PRODUCTS_PART_FACTOR -dim 2 tdf4 products
}

# For fused conv-conv layers we need to partition the L2 filter data s well.
if {$layer_type == "conv-conv"} {
   set l2_filter_part [expr {int(ceil($l2_mult_unroll / 2.0))}]
   if {$l2_filter_part > 1} {
      set_directive_array_partition -type cyclic -factor $l2_filter_part -dim 1 $filter_top $filter_data_2
   }
}

# readInputs directives
# Which exact loop we pipeline and unroll depends on the scale factor, since the scale factor
# can exceed the trip count of the inner most loop.
if {$READ_SCALE_FACTOR < 16} {
   set unroll $READ_SCALE_FACTOR
   set_directive_pipeline tdf4_readInputs/IL6
   set_directive_unroll -factor $unroll tdf4_readInputs/IL6
} elseif {$READ_SCALE_FACTOR == 16} {
   set_directive_pipeline tdf4_readInputs/IL5
} else {
   # Currently do not support unrolling readInputs loops beyond a scale factor of # input channels.
   puts "Invalid read scale factor."
   exit 2
}

# readFilters directives
if {$READ_SCALE_FACTOR < 16} {
   set_directive_pipeline tdf4_readFilters/FL6
   set_directive_unroll -factor $READ_SCALE_FACTOR tdf4_readFilters/FL6
} elseif {$READ_SCALE_FACTOR == 16} {
   set_directive_pipeline tdf4_readFilters/FL5
}

# Pipeline and unroll dot product multiplications
if {$READ_SCALE_FACTOR < 16} {
   set_directive_pipeline tdf4_dot_product/DP_OUTER_3
   set_directive_unroll -factor $READ_SCALE_FACTOR tdf4_dot_product/DP_OUTER_3
} elseif {$READ_SCALE_FACTOR == 16} {
   set_directive_pipeline tdf4_dot_product/DP_OUTER_2
}

# Unroll the adjustment loop if appropriate
if {$UNROLL_ADJUST_LOOP} {
   set_directive_unroll -factor 2 tdf4_adjust/ADJUST_LOOP
}

# Use dataflow pipelining for the top-level loop.
set_directive_dataflow tdf4/TOP_LOOP

