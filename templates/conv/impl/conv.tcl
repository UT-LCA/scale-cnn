###############################################################################
#
#  ${lname}.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the layer
#  Run this TCL file if you just want to synthesize this one layer.
#  To execute this, use this command:
#     vivado_hls -f run_hls.tcl
#
###############################################################################

# Create a Vivado HLS project
open_project -reset ${lname}_prj
add_files -cflags "-I .." {../global_defines.h ${lname}.cpp}
add_files -tb -cflags "-I .." {../test/golden.cpp ../test/tb_${lname}.cpp}
set_top ${lname}

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {$fpga_part}
create_clock -period 10

# Apply the directives
source ${lname}_impl_directives.tcl
source ../${lname}_common_directives.tcl

# Simulate the design
csim_design -clean

# Run Synthesis
csynth_design

exit



