###############################################################################
#
#  ${name}.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the $name network.
#
#  Run this TCL file if you just want to synthesize the entire network.
#
###############################################################################

set COMMON_DIR $$env(SCALE_CNN_ROOT)/common
set COMMON_TEST_DIR $$env(SCALE_CNN_ROOT)/common/test

# Create a Vivado HLS project
open_project -reset ${name}_prj
add_files -cflags "-I .. -I $$COMMON_DIR"  "$$COMMON_DIR/global_defines.h ../${name}.cpp"
set_top ${lname}_top

set top ${name}


# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {$fpga_part}
create_clock -period 10

# For each layer, add its files and source its directives.
source ${name}_layers.tcl

# Run Synthesis
# Disable dataflow canonical form warnings
config_dataflow -strict_mode off
csynth_design

exit



