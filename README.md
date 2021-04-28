# Scale-CNN Overview

Scale-CNN is a tool for generating high-throughput CNN inference accelerators on Xilinx Ultrascale+ FPGAs. The accelerators use a "network pipeline" architecture where each layer has its own dedicated FPGA resources, separated from other layers by double-buffered feature map RAMs. This enables all layers to work on a different inference simultaneously in pipelined fashion. The large FPGAs are used so that all feature maps and weights can be stored on-chip.

Scale-CNN works by generating multiple possible implementations for each layer in the network. It synthesizes each layer with Vitis HLS to get resource utilization and performance estimates and then uses those to determine the Pareto-optimal design points. It can then generate multiple implementations for the network by choosing different permutations of layer design points and putting them together to make an accelerator for the entire network. The permutations are chosen intelligently such that the layer latencies are balanced -- this maximizes resource efficiency.

Scale-CNN is intended to work for any CNN, but currently only has a minimal feature set to support basic convolution and maxpool layers. It was primarily designed around the [Tiny Darknet](https://pjreddie.com/darknet/tiny-darknet/) neural network.

# Requirements

The following is required to use Scale-CNN:

- Unix system with GNU Core Utilities (only tested on CentOS 7)
- python3 and pip3. Virtual environment support also required if you do not have admin privileges.
- Vitis HLS v2020.2, with `vitis_hls` executable directory added to `PATH`

# Directory Structure

- `common/` contains files that can be shared among all layer implementations of any layer type
- `fpgas/` contains a single file, `fpgas.json`, that stores on-chip resources for different FPGAs. This data can be found in Xilinx Product Tables
- `layers/` contains some layer JSON files used to test individual layers. You should not need this to synthesize entire networks.
- `networks/` contains JSON files used to describe entire networks. These show how network dimensions and options are provided to the tool.
- `scripts/` contains all of the Python scripts for the tool. This is where you can find the top-level script, `scale-cnn.py`
- `templates/` contains the template files for different layers. These template files have replacement tokens that are replaced with specific values for each layer or layer implementation
- `vtr/` contains some tool-generated artifacts for a separate research project external to Scale-CNN.


