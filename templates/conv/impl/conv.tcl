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
add_files -cflags "-I .. -I $$COMMON_DIR -D FAST_COMPILE=$fast_compile"  "$$COMMON_DIR/global_defines.h ${lname}.cpp"
add_files -tb -cflags "-I .. -I $$COMMON_TEST_DIR -I $$COMMON_DIR" "$$COMMON_TEST_DIR/tb_utils.cpp ../test/golden.cpp ../test/tb_${lname}.cpp"
set_top ${lname}_top

# Set variables for the optimization directives
set top ${lname}_top
set in_data in_data
set out_data out_data
set filter_data filter_data
set layer_type "conv"
set final_layer 1

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {$fpga_part}
create_clock -period $target_clock_period

# Apply the directives
source ${lname}_impl_directives.tcl
source ../${lname}_conv_directives.tcl

# Configure floating point operation latencies 
set reduce_dsp_usage $reduce_dsp_usage
if {$$reduce_dsp_usage} {
   config_op hadd -impl meddsp  -latency $hadd_latency
   config_op hmul -impl fulldsp -latency $hmul_latency
} else {
   config_op hadd -impl fulldsp -latency $hadd_latency
   config_op hmul -impl maxdsp  -latency $hmul_latency
}

# Simulate the design
csim_design -clean

# Run Synthesis
# Disable dataflow canonical form warnings
config_dataflow -strict_mode off
csynth_design

exit



