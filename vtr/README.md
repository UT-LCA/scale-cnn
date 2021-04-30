# VTR

This directory contains files for using Scale-CNN generated artifacts with VTR.

`fix_verilog` contains files and scripts needed to modify the HLS-generated Verilog code to be compatible with VTR. There is another README in here with more details.

`large` and `small` are all the files needed to generate two different implementations of Tiny Darknet with fusing. `small` uses no loop unrolling whatsoever while `large` has unrolling in multiple layers. The large design will use more resources but has higher throughput.

In each of these, we see two directories and a JSON file. The JSON file holds information for all possible network implementations found by the tool in the `analyze\_network\_options` step, where it is generated. For each network implementation, it stores information like which layer implementations it is using, estimated cost and performance, etc.

## layers directory

The `layers` directory stores all of the files for individual layers. Inside you will find subdirectories for each layer of the network, e.g. "tdf1" for the first layer. Inside one of these you will see some files that are shared among all layer implementations, and directories corresponding to each implementation. For example, the file "tdf1\_common\_defines.h" is shared by all layer implementations. The directory "r1\_o1" is the directory for the layer implementation of tdf1 where both scale factors are 1. There is also a "test" directory which has the files using for running the C Simulation step prior to HLS synthesis. These test files are also shared by all layer implementations.

Inside the layer implementation folder (using `r1_o1` as an example), we see the files that are specific to this layer implementation.
- `impl_info.json` is a small JSON file with data on this implementation that is consumed by the tool. You can ignore this file.
- `run.sh` holds the command you need to run to synthesize this layer implementation.
- `tdf1.cpp` is the top-level CPP file for this layer implementation
- `tdf1.tcl` is the top-level TCL file for this layer implementation. This is the one that gets passed to `vitis_hls` as a command-line argument.
- The other `.h` and `.tcl` files are referenced / included by the top-level ones.
- `viewreport.sh` is a shortcut for opening a certain report file in the directory that holds all the HLS-generated reports after synthesis.
- `vitis_hls.log` is the log when synthesis of this layer was previously ran.


## impls directory

The `impls` directory holds files for different network implementations. Recall that each network implementation is characterized only by a permutation of layer implementations.

Files that pertain to individual layers are not duplicated or copied to `impls`. Instead, the files in `impls` use relative paths that "point" to the layer implementations that they use.

Let's look inside the `impls` directory under `large`. Here, we see two more directories: `i10` and `include`.
The `include` directory just contains header files for the top-level functions in each layer. These are shared among all network implementations.
The `i10` directory holds all files for the network implementation named "i10", a name that was auto-assigned by the tool during `analyze_network_options`.

Inside the `i10` directory, we see the following files:
- `out.log`: This is a duplicate of `vitis_hls.log`. Ignore.
- `td_fused.cpp`: This is the top-level CPP file for the entire network. If you look inside here, you will see the function calls to the individual layer functions.
- `td_fused.tcl`: This is the top-level TCL file for the entire network. This is what gets passed to `vitis_hls` as a command-line argument.
- `td_fused_layers.h`: A file that just contains includes for the headers in the `include` directory discussed earlier.
- `td_fused_layers.tcl`: This TCL file is called by the top-level one. This is the TCL file that holds the paths to all of layer implementations used by this network implementation. One by one, it sets some variables and then calls each layer's top-level TCL file, so that its directives get applied to the entire design. It adds the top-level CPP file for each layer to the design as well.
- `vitis_hls.log`: The log when the synthesis of this layer was previously ran.



