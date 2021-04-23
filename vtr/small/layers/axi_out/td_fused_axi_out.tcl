###############################################################################
#
#  td_fused_axi_out.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the AXI output "layer"
#
###############################################################################

set COMMON_DIR $env(SCALE_CNN_ROOT)/common

# Create a Vivado HLS project
open_project -reset td_fused_axi_out_prj
add_files -cflags "-I .. -I $COMMON_DIR"  "td_fused_axi_out.cpp"
set_top td_fused_axi_out_top

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {xcvu3p-ffvc1517-3-e}
create_clock -period 10

# Apply the directives
# Feature maps are stored in URAMs
set_directive_bind_storage -type ram_s2p -impl uram td_fused_axi_out_top fmaps
# Reshape the fmaps array into 4 words per row
set_directive_array_reshape -type cyclic -factor 4 td_fused_axi_out_top fmaps -dim 3

# Run Synthesis
csynth_design

exit

