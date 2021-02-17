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

# Create a Vivado HLS project
open_project -reset ${name}_prj
add_files -cflags "-I $$COMMON_DIR -I ../../layers/axi_in/ -I ../../layers/axi_out/"  "${name}.cpp"
set_top ${name}_top

set top $name
set filter_top ${name}_top

# Create solution, specify FPGA and desired clock period.
open_solution -reset "solution1"
set_part {$fpga_part}
create_clock -period $target_clock_period

# For each layer, add its files and source its directives.
source ${name}_layers.tcl

# Configure floating point operation latencies 
set reduce_dsp_usage $reduce_dsp_usage
if {$$reduce_dsp_usage} {
   config_op hadd -impl meddsp  -latency $hadd_latency
   config_op hmul -impl fulldsp -latency $hmul_latency
} else {
   config_op hadd -impl fulldsp -latency $hadd_latency
   config_op hmul -impl maxdsp  -latency $hmul_latency
}

# Set the whole network to use dataflow
set_directive_dataflow ${name}

# Run Synthesis
# Disable dataflow canonical form warnings
config_dataflow -strict_mode off
csynth_design

exit



