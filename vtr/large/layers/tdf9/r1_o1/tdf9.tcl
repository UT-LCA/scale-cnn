###############################################################################
#
#  tdf9.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the layer
#  Run this TCL file if you just want to synthesize this one layer.
#  To execute this, use this command:
#     vitis_hls -f tdf9.tcl
#
###############################################################################

set COMMON_DIR $env(SCALE_CNN_ROOT)/common
set COMMON_TEST_DIR $env(SCALE_CNN_ROOT)/common/test

# Create a Vivado HLS project
open_project -reset tdf9_prj
add_files -cflags "-I .. -I $COMMON_DIR -D FAST_COMPILE=0"  "$COMMON_DIR/global_defines.h tdf9.cpp"
add_files -tb -cflags "-I .. -I $COMMON_TEST_DIR -I $COMMON_DIR" "$COMMON_TEST_DIR/tb_utils.cpp ../test/golden.cpp ../test/tb_tdf9.cpp"
set_top tdf9_top

# Set variables for the optimization directives
set top tdf9_top
set filter_top tdf9_top
set in_data in_data
set out_data out_data
set filter_data filter_data
set adjustments adjustments
set layer_type "conv"
set final_layer 1

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {xcvu3p-ffvc1517-3-e}
create_clock -period 3

# Apply the directives
source tdf9_impl_directives.tcl
source ../tdf9_conv_directives.tcl

# Configure floating point operation latencies 
set reduce_dsp_usage False
if {$reduce_dsp_usage} {
   config_op hadd -impl meddsp  -latency 7
   config_op hmul -impl fulldsp -latency 4
} else {
   config_op hadd -impl fulldsp -latency 7
   config_op hmul -impl maxdsp  -latency 4
}

# Simulate the design
#csim_design -clean

# Run Synthesis
# Disable dataflow canonical form warnings
config_dataflow -strict_mode off
csynth_design

exit



