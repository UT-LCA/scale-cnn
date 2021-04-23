###############################################################################
#
#  tdf11.tcl -- AUTO-GENERATED --
#  
#  Top-level TCL file for synthesizing the layer
#  Run this TCL file if you just want to synthesize this one layer.
#  To execute this, use this command:
#     vitis_hls -f tdf11.tcl
#
###############################################################################

set COMMON_DIR $env(SCALE_CNN_ROOT)/common
set COMMON_TEST_DIR $env(SCALE_CNN_ROOT)/common/test

# Create a Vivado HLS project
open_project -reset tdf11_prj
add_files -cflags "-I .. -I $COMMON_DIR -D FAST_COMPILE=0"  "$COMMON_DIR/global_defines.h tdf11.cpp"
add_files -tb -cflags "-I .. -I $COMMON_TEST_DIR -I $COMMON_DIR" "$COMMON_TEST_DIR/tb_utils.cpp ../test/golden.cpp ../test/tb_tdf11.cpp"
set_top tdf11_top

# Set variables for the optimization directives
set top tdf11_top
set filter_top tdf11_top
set in_data in_data
set out_data out_data
set filter_data l1_filter_data
set filter_data_2 l2_filter_data
set adjustments l1_adjustments
set adjustments_2 l2_adjustments
set layer_type "conv-conv"
set final_layer 1

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {xcvu3p-ffvc1517-3-e}
create_clock -period 3

# Apply the directives
source tdf11_impl_directives.tcl
source ../tdf11_conv_directives.tcl

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



