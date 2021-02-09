###############################################################################
#
#  ${name}_axi_in.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the AXI input "layer"
#
###############################################################################

set COMMON_DIR $$env(SCALE_CNN_ROOT)/common

# Create a Vivado HLS project
open_project -reset ${name}_axi_in_prj
add_files -cflags "-I .. -I $$COMMON_DIR"  "$$COMMON_DIR/global_defines.h ${name}_axi_in.cpp"
set_top ${name}_axi_in_top

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {$fpga_part}
create_clock -period 10

# Apply the directives
# Feature maps are stored in URAMs
set_directive_bind_storage -type ram_s2p -impl uram ${name}_axi_in_top fmaps
# Reshape the fmaps array into 4 words per row
set_directive_array_reshape -type cyclic -factor 4 ${name}_axi_in_top fmaps -dim 3

# Run Synthesis
csynth_design

exit

