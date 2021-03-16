
# Declare all of the layers
set layers [list \
$layer_dicts
]

set num_layers [llength $$layers]

for {set i 0} {$$i < $$num_layers} {incr i} {
   puts "Configuring layer $$i"
   # Get the name and path of this layer
   set layer_name [dict get [lindex $$layers $$i] name]
   set layer_impl_path [dict get [lindex $$layers $$i] path]
   set layer_type [dict get [lindex $$layers $$i] type]
   # Set memory array names and other variables
   set in_data "$${layer_name}_fmaps"
   set filter_data "$${layer_name}_filters"
   set adjustments "$${layer_name}_adjustments"
   if {$$layer_type == "conv-conv"} {
      set filter_data_2 "$${layer_name}_l2_filters"
      set adjustments_2 "$${layer_name}_l2_adjustments"
   }
   set final_layer [expr $$i == ($$num_layers-1)]
   if {$$final_layer == 1} {
      set out_data final_fmaps
   }
   # Add the CPP file for this layer.
   add_files -cflags "-I $$layer_impl_path -I $$COMMON_DIR" "$$layer_impl_path/$${layer_name}.cpp"
   # Source the TCL files for this layer.
   source $${layer_impl_path}/$${layer_name}_impl_directives.tcl
   # TODO: This won't work when we have other layer types.
   source $${layer_impl_path}/../$${layer_name}_conv_directives.tcl
}

puts "Configuring AXI I/O layers."
add_files -cflags "-I ../../layers/axi_in/ -I $$COMMON_DIR" "../../layers/axi_in/${name}_axi_in.cpp"
add_files -cflags "-I ../../layers/axi_out/ -I $$COMMON_DIR" "../../layers/axi_out/${name}_axi_out.cpp"
