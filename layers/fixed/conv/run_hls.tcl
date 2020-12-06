############################################################
#
# This file is generated automatically by cnn-pragma-sel.
#
############################################################

# Create a Vivado HLS project
open_project -reset conv_prj
add_files {common.h conv.c}
set_top conv

# Solution1 *************************
open_solution -reset "solution1"
set_part {xcku115-flvf1924-3-e}
create_clock -period 10

# Apply the directives
source directives.tcl

# Run Synthesis
csynth_design

exit



