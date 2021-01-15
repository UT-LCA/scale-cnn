
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
   # Set memory array names and other variables
   set in_data "$${layer_name}_fmaps"
   set filter_data "$${layer_name}_filters"
   set final_layer [expr i == ($$num_layers-1)]
   if {$$final_layer == 1} {
      set out_data final_fmaps
   }
   # Add the CPP file for this layer.
   add_files -cflags "-I $$layer_impl_path -I $$layer_impl_path/.." "$${layer_name}.cpp"
   # Source the TCL files for this layer.
   source $${layer_impl_path}/$${layer_name}_impl_directives.tcl
   # TODO: This won't work when we have other layer types.
   source $${layer_impl_path}/../$${layer_name}_conv_directives.tcl
}
