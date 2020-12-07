############################################################
#
#  ${lname}.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the layer
#  To execute this, use this command:
#     vivado_hls -f run_hls.tcl
#
############################################################

# Create a Vivado HLS project
open_project -reset conv_prj
add_files {${lname}.c}
set_top ${lname}

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {$fpga_part}
create_clock -period 10

# Apply the directives
source ${lname}_impl_directives.tcl
source ${lname}_common_directives.tcl

# Run Synthesis
csynth_design

exit



