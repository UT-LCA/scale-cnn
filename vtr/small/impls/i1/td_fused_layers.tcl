
# Declare all of the layers
set layers [list \
   [dict create name tdf1 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf1/r1_o1 type conv-max] \
   [dict create name tdf2 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf2/r1_o1 type conv-max] \
   [dict create name tdf3 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf3/r1_o1 type conv] \
   [dict create name tdf4 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf4/r1_o1 type conv-conv] \
   [dict create name tdf5 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf5/r1_o1 type conv-max] \
   [dict create name tdf6 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf6/r1_o1 type conv] \
   [dict create name tdf7 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf7/r1_o1 type conv-conv] \
   [dict create name tdf8 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf8/r1_o1 type conv-max] \
   [dict create name tdf9 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf9/r1_o1 type conv] \
   [dict create name tdf10 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf10/r1_o1 type conv-conv] \
   [dict create name tdf11 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf11/r1_o1 type conv-conv] \
   [dict create name tdf12 path /home/ecelrc/students/drauch/research/scale-cnn/networks/td_fused/layers/tdf12/r1_o1 type conv] \

]

set num_layers [llength $layers]

for {set i 0} {$i < $num_layers} {incr i} {
   puts "Configuring layer $i"
   # Get the name and path of this layer
   set layer_name [dict get [lindex $layers $i] name]
   set layer_impl_path [dict get [lindex $layers $i] path]
   set layer_type [dict get [lindex $layers $i] type]
   # Set memory array names and other variables
   set in_data "${layer_name}_fmaps"
   set filter_data "${layer_name}_filters"
   set adjustments "${layer_name}_adjustments"
   if {$layer_type == "conv-conv"} {
      set filter_data_2 "${layer_name}_l2_filters"
      set adjustments_2 "${layer_name}_l2_adjustments"
   }
   set final_layer [expr $i == ($num_layers-1)]
   if {$final_layer == 1} {
      set out_data final_fmaps
   }
   # Add the CPP file for this layer.
   add_files -cflags "-I $layer_impl_path -I $COMMON_DIR" "$layer_impl_path/${layer_name}.cpp"
   # Source the TCL files for this layer.
   source ${layer_impl_path}/${layer_name}_impl_directives.tcl
   # TODO: This won't work when we have other layer types.
   source ${layer_impl_path}/../${layer_name}_conv_directives.tcl
}

puts "Configuring AXI I/O layers."
add_files -cflags "-I ../../layers/axi_in/ -I $COMMON_DIR" "../../layers/axi_in/td_fused_axi_in.cpp"
add_files -cflags "-I ../../layers/axi_out/ -I $COMMON_DIR" "../../layers/axi_out/td_fused_axi_out.cpp"
