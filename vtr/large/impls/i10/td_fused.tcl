###############################################################################
#
#  td_fused.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the td_fused network.
#
#  Run this TCL file if you just want to synthesize the entire network.
#
###############################################################################

set COMMON_DIR $env(SCALE_CNN_ROOT)/common

# Create a Vivado HLS project
open_project -reset td_fused_prj
add_files -cflags "-I $COMMON_DIR -I ../include/"  "td_fused.cpp"
set_top td_fused_top

set top td_fused
set filter_top td_fused_top

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {xcvu3p-ffvc1517-3-e}
create_clock -period 3

# For each layer, add its files and source its directives.
source td_fused_layers.tcl

# Configure floating point operation latencies 
set reduce_dsp_usage False
if {$reduce_dsp_usage} {
   config_op hadd -impl meddsp  -latency 7
   config_op hmul -impl fulldsp -latency 4
} else {
   config_op hadd -impl fulldsp -latency 7
   config_op hmul -impl maxdsp  -latency 4
}

# Set the whole network to use dataflow
set_directive_dataflow td_fused

# Run Synthesis
# Disable dataflow canonical form warnings
config_dataflow -strict_mode off
csynth_design

exit



