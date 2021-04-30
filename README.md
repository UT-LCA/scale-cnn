# Scale-CNN

## Scale-CNN Overview

Scale-CNN is a tool for generating high-throughput CNN inference accelerators on Xilinx Ultrascale+ FPGAs. The accelerators use a "network pipeline" architecture where each layer has its own dedicated FPGA resources, separated from other layers by double-buffered feature map RAMs. This enables all layers to work on a different inference simultaneously in pipelined fashion. The large FPGAs are used so that all feature maps and weights can be stored on-chip.

Scale-CNN works by generating multiple possible implementations for each layer in the network. It synthesizes each layer with Vitis HLS to get resource utilization and performance estimates and then uses those to determine the Pareto-optimal design points. It can then generate multiple implementations for the network by choosing different permutations of layer design points and putting them together to make an accelerator for the entire network. The permutations are chosen intelligently such that the layer latencies are balanced -- this maximizes resource efficiency.

Scale-CNN is intended to work for any CNN, but currently only has a minimal feature set to support basic convolution and maxpool layers. It was primarily designed around the [Tiny Darknet](https://pjreddie.com/darknet/tiny-darknet/) neural network. Additionally, it currently only supports networks with simple feed-forward topologies. Topologies with feedback, divergence and convergence are not supported.

## Requirements

The following is required to use Scale-CNN:

- Unix system with GNU Core Utilities (only tested on CentOS 7)
- python3 and pip3. Virtual environment support also required if you do not have admin privileges.
- Vitis HLS v2020.2, with `vitis_hls` executable directory added to `PATH`

With these satisfied, the only step required for setup is to set the environment variable `SCALE_CNN_ROOT` to the top-level directory of the repo.

## Directory Structure

- `doc/` holds documentation
- `common/` contains files that can be shared among all layer implementations of any layer type
- `fpgas/` contains a single file, `fpgas.json`, that stores on-chip resources for different FPGAs. This data can be found in Xilinx Product Tables
- `layers/` contains some layer JSON files used to test individual layers. You should not need this to synthesize entire networks.
- `networks/` contains JSON files used to describe entire networks. These show how network dimensions and options are provided to the tool.
- `scripts/` contains all of the Python scripts for the tool. This is where you can find the top-level script, `scale-cnn.py`
- `templates/` contains the template files for different layers and networks. These template files have replacement tokens that are replaced with specific values for each implementation.
- `vtr/` contains some tool-generated artifacts for a separate research project external to Scale-CNN.

## Usage

All commands must be run under the `scripts/` directory.

To get detailed info on command-line arguments:
```
python3 scale-cnn.py -h
```

### Single Layer Generation

Use these commands when working with individual layers rather than entire networks.

Generating implementations for a layer:

```
python3 scale-cnn.py gen_layer -l ../layers/examples/tiny_darknet_conv1.json -o ../layers/td_conv1
```

This will generate all the files needed for the different implementations of the layer, but will not synthesize them. To synthesize them:

```
python3 scale-cnn.py explore_layer -i ../layers/td_conv1/td_conv1_implementations.txt &> out.log &
```

Since the synthesizing can take a while, you should pipe all output to a log file which you can then monitor with `tail -f out.log`. Once the syntheses have finished, you can analyze the results:

```
python3 scale-cnn.py explore_layer -l ../layers/examples/tiny_darknet_conv1.json -i ../layers/td_conv1/td_conv1_implementations.txt -ss --cost_function default
```

The synthesis command does the same thing, but this lets you skip the lengthy syntheses with `-ss` (skip synthesis) and lets you choose a different cost function without having to re-synthesize everything. In retrospect, these probably should have been split up as separate commands, since it is a bit confusing. The "explore\_layer" command both performs the synthesis step ands run the analysis. To just run the analysis after synthesis has finished without running all the syntheses again, you run "explore\_layer" again while skipping synthesis.

### Network Generation

Use these commands when working with entire networks.

To generate the layer implementations for each layer in the network:

```
python3 scale-cnn.py gen_network_layers --networkspec ../networks/tiny_darknet_fused.json --output ../networks/td_fused/
```

To synthesize all implemenations for all layers:

```
python3 scale-cnn.py synth_network_layers --networkspec ../networks/tiny_darknet_fused.json -i ../networks/td_fused/ &> out.log &
```

This step can take a very long time, on the order of several hours. You should definitely pipe the command to a logfile as shown and monitor it with `tail -f out.log`. 

Sometimes, synthesizing a layer can sporadically fail. Typically this is because it got stuck somewhere, took too long and timed out. Currently, the tool sets a 30 minute timeout on each layer synthesis. If a layer synthesis times out, it tries it again once. If it times out again, the tool exits and does not kick off any further layer synthesis runs. This timeout duration can be adjusted in `synthesize_layer()` inside `hls.py` if desired.

If layer synthesis fails part way through the layers of a network, you don't have to start all over from the first layer. Instead, you can use the `--layer_offset` argument to tell the tool to skip to that layer. For example, if the implementations for layers 1-6 complete fine but then something fails on layer 7, just run the same command again with `--layer_offset 7` to pick up where the tool left off (though any implementations in layer 7 that previously succeeded will need to be synthesized again).

To analyze the synthesis results and see potential network implementation options:

```
python3 scale-cnn.py analyze_network_options --networkspec ../networks/tiny_darknet_fused.json --input ../networks/td_fused/ --network_options 100
```

This step requires the `scikit-learn` module to be installed. If it is not installed, and you have admin privileges, you can install it simply with `pip3 install scikit-learn`. You may also need to `pip3 install Cython` first. If you do not have admin privileges, you can install these modules in a virtual environment. Simply run `create_venv.sh` once on the machine. Each time you want to run the `analyze_network_options` command, precede it with `source env/bin/activate` and follow it with `deactivate`.

The `--network_options` command tells the tool to limit all potential network implementations (which can be many) to a number of "recommendations" that you specify.

The `analyze_network_options` command will create a summary that allows you to look at the potential network implementations, but does not actually generate them. To generate one:

```
python3 scale-cnn.py gen_network_implementations --networkspec ../networks/tiny_darknet_fused.json -i ../networks/td_fused/ --impl i1
```

The network implementation files can now be found in the network's directory under `impls/i1/`. Synthesizing the network is done outside the tool, as follows:

```
vitis_hls -f td_fused.tcl &> out.log &
```

This step can take several minutes to over an hour.


## Layer / Network Implementation Names

Scale-CNN generates multiple possible implementations for both layers and networks. Different implementations are given different names. The naming schemes are slightly different for layers and networks.

A layer implementation name will looks like "r2\_o4". As mentioned in the paper, each layer implementation is characterized by two values -- input channel scale factor and output channel scale factor. The numbers next to "r" and "o" represent these two scale factors, respectively. The reason it is "r" instead of "i" is because I had originally called it the "read scale factor" and then never got around to changing it in the code.

A network implementation name will look like "i3". In this case, "i" just stands for "implementation", and the number does not represent anything of significance; it is just used to distinguish itself from other implementations.

## Layer Types

Currently, the only layer types supported by Scale-CNN are:

- `conv`: Simple convolution layer.
- `conv-max`: Fused convolution-maxpool layer.
- `conv-conv`: Two convolution layers fused together to reduce memory requirements
- `axi_in` / `axi_out`: These are "pseudo-layers" that exist at the beginning and end of the network pipeline to connect the accelerator to an AXI4-Lite bus. These are automatically created by the tool and should not be included in the network JSON file.

In addition to the weights and biases, all convolution layers are designed to have per-filter batch normalization constants (mean and 1/stddev). These can be set to 0 and 1 respectively if no batch normalization is required; however, a good optimization to make in the future would be to extend the tool to make this optional.

It should also be noted that Scale-CNN only supports simple feed-forward network topologies; networks that use feedback or divergence and convergence are not supported.

# Additional limitations

These limitations currently exist in the tool and could be removed with further development:

- The number of channels in any layer must either be exactly 3 or a multiple of 4. 
- Convolution stride must be 1. Supporting larger values is nominally supported but has not been tested.
- For maxpool layers, stride must equal pooling size.
- For fused conv-conv layers, the second layer must have filter size = 1. The first layer can have any filter size.

Lastly, the Vitis HLS toolchain currently does a really bad job of array partitioning or reshaping with any factor that is not
a power of 2 (this is why the first limitation exists). If you try to extend this tool, do everything you can to make sure
that this is satisfied.
