###############################################################################
#
#  ${lname}.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the layer
#  Run this TCL file if you just want to synthesize this one layer.
#  To execute this, use this command:
#     vitis_hls -f ${lname}.tcl
#
###############################################################################

set COMMON_DIR $$env(SCALE_CNN_ROOT)/common
set COMMON_TEST_DIR $$env(SCALE_CNN_ROOT)/common/test

# Create a Vivado HLS project
open_project -reset ${lname}_prj
add_files -cflags "-I .. -I $$COMMON_DIR"  "$$COMMON_DIR/global_defines.h ${lname}.cpp"
add_files -tb -cflags "-I .. -I $$COMMON_TEST_DIR" "$$COMMON_TEST_DIR/tb_utils.cpp ../test/golden.cpp ../test/tb_${lname}.cpp"
set_top ${lname}_top

# Set variables for the optimization directives
set top ${lname}_top
set in_data in_data
set out_data out_data
set filter_data filter_data
set final_layer 1

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {$fpga_part}
create_clock -period 10

# Apply the directives
source ${lname}_impl_directives.tcl
source ../${lname}_conv_directives.tcl

# Simulate the design
csim_design -clean

# Run Synthesis
# Disable dataflow canonical form warnings
config_dataflow -strict_mode off
csynth_design

exit



