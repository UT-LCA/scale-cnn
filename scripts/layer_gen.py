# layer_gen.py
# Contains the code for generating the files needed for a layer
# given its template and its specification
from functools import reduce
import copy
import os
import accum
import math
import utils

def get_conv_layer_files(layer_name):
   # Each tuple is ([name of file to be created], [name of template file])
   # List of once-per-layer files
   layer_files = [('{}_common_defines.h'.format(layer_name), 'common_defines.h'), \
                  ('{}_conv_directives.tcl'.format(layer_name), '../conv_shared/conv_directives.tcl'), \
                  ('test/tb_{}.cpp'.format(layer_name), 'test/testbench.cpp'), \
                  ('test/golden.cpp', 'test/golden.cpp')]

   return layer_files

def get_conv_layer_impl_files(layer_name, layer_type):
   # List of once-per-layer-implementation files
   impl_files = [('{}.cpp'.format(layer_name), '{}.cpp'.format(layer_type)), \
                 ('{}_conv_stages.h'.format(layer_name), '../../conv_shared/conv_stages.h'), \
                 ('{}.tcl'.format(layer_name), '{}.tcl'.format(layer_type)), \
                 ('{}_impl_defines.h'.format(layer_name), 'impl_defines.h'), \
                 ('{}_impl_directives.tcl'.format(layer_name), 'impl_directives.tcl'), \
                 ('run.sh', 'run.sh'),
                 ('viewreport.sh', 'viewreport.sh')]
   return impl_files
   
def gen_layer_files(layer_spec, layer_files, odir, template_path):
   for layer_file, template_fname in layer_files:
      template_fp = os.path.join(template_path, template_fname)
      output_fp = os.path.join(odir, layer_file)
      utils.make_file_from_template(template_fp, output_fp, layer_spec)

def gen_layer_impl_files(layer_spec, impl_files, impl, odir, template_path):
   impl_dir = os.path.join(odir, impl['name'])
   if not os.path.isdir(impl_dir):
      os.mkdir(impl_dir)
   for impl_file, template_fname in impl_files:
      template_fp = os.path.join(template_path, 'impl', template_fname)
      output_fp = os.path.join(odir, impl['name'], impl_file)
      substitutions = copy.copy(layer_spec)
      substitutions.update(impl)
      utils.make_file_from_template(template_fp, output_fp, substitutions)
   return impl_dir

def get_axi_io_layer_files(name, d):
   layer_files = [('{}_axi_{}.cpp'.format(name, d), 'axi_{}.cpp'.format(d)),
                  ('{}_axi_{}.h'  .format(name, d), 'axi_{}.h'  .format(d)),
                  ('{}_axi_{}.tcl'.format(name, d), 'axi_{}.tcl'.format(d)),
                  ('run.sh', 'run.sh'),
                  ('viewreport.sh', 'viewreport.sh')]
   return layer_files

# Return a sorted list of all the integer factors of a positive integer
# Used to determine options for unroll factors.
def factors(n):
   x = list(set(reduce(list.__add__, 
               ([i, n//i] for i in range(1, int(n**0.5) + 1) if n % i == 0))))
   x.sort()
   return x


# If number of channels is a multiple of 4, choose 4.
# Otherwise, if a multiple of 3, choose 3.
# Otherwise - invalid layer configuration. We cannot currently handle other cases.
def GetUramWordsPerRow(chans):
   if chans % 4 == 0:
      return 4
   elif chans % 3 == 0:
      return 3
   else:
      raise Exception('Invalid # channels: {}'.format(chans))

# Estimates the total execution time of a convolution layer
# given its dimensions and scale factors
def GetConvEstimatedLatency(layer_spec, read_sf, ochan_sf):
   # Total latency = II * top loop iterations + pipeline depth
   # The number of top loop iterations is:
   #  O_H * O_W * O_CHANS / OCHAN_SF for ordinary conv layers
   #  O_H * O_W * O_CHANS * (POOLING_FACTOR^2) / OCHAN_SF for fused conv-max layers
   #  O_H * O_W * INTERMEDIATE_CHANS / OCHAN_SF for fused conv-conv conv layers
   OH = layer_spec['output_height']
   OW = layer_spec['output_width']
   OC = layer_spec['intermediate_chans'] if layer_spec['layer_type'] == 'conv-conv' else layer_spec['output_chans']
   P  = 1 if layer_spec['layer_type'] != 'conv-max' else layer_spec['pooling_factor']
   top_loop_iters = int(OH*OW*OC*P*P / ochan_sf)
   # The initiation interval is the latency of the longest stage, plus one for dataflow
   # pipeline overhead. If everything synthesizes correctly, the longest stage should 
   # always be dot_product which should take exactly INPUT_CHANS*(FILTER_SIZE^2) / read_sf + 3 cycles.
   IC = layer_spec['input_chans']
   FS = layer_spec['filter_size']
   II = int(FS*FS*IC / read_sf) + 3 + 1
   # The pipeline depth is harder to estimate, but it doesn't really matter, because since
   # the total number of iterations is typically very large, it is very small in comparison,
   # so a poor estimation will barely harm the overall estimate accuracy.
   # I will estimate that the latency is II*3 for conv and conv-max and II*6 for conv-conv
   # because there are three extra stages in the layer pipeline.
   if layer_spec['layer_type'] == 'conv-conv':
      pipeline_depth = II*6
   else:
      pipeline_depth = II*3
   return II*top_loop_iters + pipeline_depth

# To avoid using an ungodly amount of resources, we must put a reasonable limit on how
# much scaling we can allow in a single layer.
# For right now, choosing 200 as the maximum.
MAX_TOTAL_SCALE_FACTOR = 200

# Establish an absolute minimum target latency for accumulation stages. We should
# skip any design point whose read / dot_product stages would require the accumulation
# stages to be even faster than this (in cycles).
# Any values faster than this will simply make it infeasible to create accumulation
# stages that do not bottleneck the rest of the layer's pipeline.
MIN_TARGET_LATENCY = 10

# Put a maximum on a single scale factor at 64.
# This is somewhat arbitrary but the point is to make sure that we don't consider
# design points that are absurdly imbalanced (like r1_o128)
MAX_SCALE_FACTOR = 64

# Return a list of all possible configurations for a conv layer
# It eliminates options that either have too large total scaling, or are outside
# a specified range of latencies.
# min and max latencies are in cycles, -1 if no limit.
# Each element is (read_sf, ochan_sf, estimated_latency)
# one_below_min means we should also include the slowest design point that is
# below the minimum latency.
def get_conv_impl_options(layer_spec, min_latency, max_latency, one_below_min=False):
   # Sometimes, when different layers of a network are particularly imbalanced,
   # we can encounter a situation where even the slowest implementation of one layer (r1_o1)
   # is faster than the fastest possible implementation of another layer. In this case, we would
   # return 0 options for the faster layer. If this happens, then make r1_o1 the only option to
   # consider for this layer and don't consider any other points.
   r1_o1_latency = GetConvEstimatedLatency(layer_spec, 1, 1)
   if r1_o1_latency < min_latency:
      return [(1, 1, r1_o1_latency)]
   # Different implementations for conv layers:
   # - Read Scale factor: Factors of input chans
   # - Output channel scale factor: Factors of output chans
   # For fused conv-conv layers, the "output channels" is the output channels of the first
   # of the two layers fused together, which is represented in "intermediate chans"
   ichans_factors = factors(layer_spec['input_chans'])
   ochans = layer_spec['intermediate_chans'] if layer_spec['layer_type'] == 'conv-conv' \
            else layer_spec['output_chans']
   ochans_factors = factors(ochans)
   # Sort and remove duplicates
   read_scale_factors  = sorted(list(set(ichans_factors)))
   ochan_scale_factors = sorted(list(set(ochans_factors)))
   options = []
   too_fast_options = []
   for read_sf in read_scale_factors:
      if read_sf > MAX_SCALE_FACTOR:
         continue
      # Make sure minimum target latency requirement is satisfied.
      vec_size = (layer_spec['filter_size'] ** 2) * layer_spec['input_chans']
      target_latency = math.ceil(vec_size / read_sf) + 3 # Three-cycle pipeline with II of 1
      if target_latency < MIN_TARGET_LATENCY:
         continue
      for ochan_sf in ochan_scale_factors:
         if ochan_sf > MAX_SCALE_FACTOR:
            continue
         # Make sure the total scaling is less than the max.
         total_scale = read_sf * ochan_sf
         if total_scale > MAX_TOTAL_SCALE_FACTOR:
            continue
         # Estimate the latency given the scale factors.
         est_lat = GetConvEstimatedLatency(layer_spec, read_sf, ochan_sf)
         opt = (read_sf, ochan_sf, est_lat)
         if max_latency != -1 and est_lat > max_latency:
            continue
         elif min_latency != -1 and est_lat < min_latency:
            too_fast_options.append(opt)
         else:
            options.append(opt)

   if one_below_min and len(too_fast_options) > 0:
      slowest = max(too_fast_options, key=lambda p: p[2])
      options.insert(0, slowest)
   return options

# Layer type generic function for getting options for a layer
def GetLayerImplOptions(layer_spec, min_latency, max_latency, one_below_min=False):
   ltype = layer_spec['layer_type']
   if ltype == 'conv' or ltype == 'conv-max' or ltype == 'conv-conv':
      options = get_conv_impl_options(layer_spec, min_latency, max_latency, one_below_min)
   else:
      raise Exception('Unknown layer type: {}'.format(ltype))
   if len(options) == 0:
      raise Exception('No layer options for layer {} met latency requirements: min {}, max {}'.format( \
         layer_spec['layer_name'], min_latency, max_latency))
   return options


# Generates the params needed for the pragmas of the L2 stages in the conv-conv
# layer pipelines
def gen_convconv_params(layer_spec, impl, target_latency):
   # The three params are all based on the factors of # intermediate channels
   # in the middle of the two conv layers fused together
   mchans = layer_spec['intermediate_chans']
   mchans_factors = sorted(list(set(factors(mchans))))
   # Ideal L2 Multiplication Loop unroll factor is the smallest factor of mchans
   # that satisfies this inequality:
   # Unroll factor >= read_sf * ochan_sf * output_chans / (filter_size^2 * input_chans)
   read_sf  = impl['read_scale_factor']
   ochan_sf = impl['ochan_scale_factor']
   ochans   = layer_spec['output_chans']
   ichans   = layer_spec['input_chans']
   fs       = layer_spec['filter_size']
   min_mul_unroll = read_sf * ochan_sf * ochans / (fs*fs*ichans)
   l2_mul_unroll = -1
   for f in mchans_factors:
      if f >= min_mul_unroll:
         l2_mul_unroll = f
         break
   # Ideal L2 Accumulation unroll factor is the smallest factor of mchans that 
   # satisfies this inequality:
   # Unroll factor >= (ochan_sf - 1) * OC * hadd latency / target latency
   # If ochan_sf = 1 this is irrelevant because this stage doesn't even exist.
   # Right now the hadd latency is fixed at 3 but this may be configurable in the future.
   hadd_latency = 3
   min_acc_unroll = (ochan_sf - 1) * ochans * hadd_latency / target_latency
   l2_acc_unroll = -1
   for f in mchans_factors:
      if f >= min_acc_unroll:
         l2_acc_unroll = f
         break
   # The partition factor of l2_products (which is between the l2_multiply and l2_accum stages)
   # must be large enough for whichever unroll factor is larger. Since the data is stored in BRAMs
   # which have two read ports each, we take the larger of the two factors and divide by 2.
   l2_prod_part = math.ceil(max(l2_mul_unroll, l2_acc_unroll) / 2)
   impl['l2_mul_unroll'] = l2_mul_unroll
   impl['l2_acc_unroll'] = l2_acc_unroll
   impl['l2_products_part_factor'] = l2_prod_part


# Generates a conv layer from the template, and generates the different implementations
# Returns a list of dicts that describe each implementation, including their directory.
def gen_conv_layer(layer_spec, odir, args):
   global MAX_TOTAL_SCALE_FACTOR
   layer_type = layer_spec['layer_type']
   template_path = os.getenv('SCALE_CNN_ROOT') + "/templates/{}/".format(layer_type)

   # Determine input and output words per URAM row.
   ichans = layer_spec['input_chans']
   ochans = layer_spec['output_chans']
   layer_spec['input_words_per_uram_row']  = 4   # Currently just always 4 since we pad to 4 if there are 3.
   layer_spec['output_words_per_uram_row'] = GetUramWordsPerRow(ochans)     
   layer_spec['input_chans_padded'] = 4 * math.ceil(ichans / 4)
   
   # Set the fast compile key
   layer_spec['fast_compile'] = int(args.fast_compile)

   # Generate the layer-specific files once
   layer_name  = layer_spec['layer_name']
   layer_files = get_conv_layer_files(layer_name)
   impl_files  = get_conv_layer_impl_files(layer_name, layer_type)
   gen_layer_files(layer_spec, layer_files, odir, template_path)
   
   # Generate all of the possible conv implementations
   min_latency = args.min_ii
   max_latency = args.max_ii
   impl_options = GetLayerImplOptions(layer_spec, min_latency, max_latency, True)
   layer_impls = []
   for read_sf, ochan_sf, est_lat in impl_options:
      impl = {}
      impl['read_scale_factor'] = read_sf
      impl['ochan_scale_factor'] = ochan_sf
      # Use the aligned writeOutputs if OCHAN_SCALE_FACTOR is a multiple of 4,
      # otherwise use unaligned.
      impl['writeFuncType'] = 'aligned' if (ochan_sf % 4 == 0) else 'unaligned'
      impl['estimated_latency'] = est_lat

      # Generate the custom code for the accumulation functions for this layer.
      # Target latency is the estimated latency of the dot_product stage, which
      # is expected to be the critical path.
      # Read bandwidth is twice the read scale factor because each BRAM has 
      # two separate read ports.
      vec_size = (layer_spec['filter_size'] ** 2) * layer_spec['input_chans']
      dp_latency = math.ceil(vec_size / read_sf) + 3 # Three-cycle pipeline with II of 1
      accum_funcs, accum_func_calls = accum.GenerateAccumulationStages( \
                                       layer_name = layer_spec['layer_name'],
                                       layer_type = layer_spec['layer_type'],
                                       ochan_sf = ochan_sf,
                                       target_latency=dp_latency, 
                                       input_length=vec_size,
                                       read_bw = read_sf*2 )

      impl['accum_functions']      = accum_funcs
      impl['accum_function_calls'] = accum_func_calls

      if layer_type == 'conv-conv':
         gen_convconv_params(layer_spec, impl, dp_latency)

      # Pick a name for the implementation
      impl['name'] = "r{}_o{}".format(read_sf, ochan_sf)
      layer_impls.append(impl)

   # Now, generate the files for each implementation
   for impl in layer_impls:
      impl_dir = gen_layer_impl_files(layer_spec, impl_files, impl, odir, template_path)
      impl['dir'] = os.path.abspath(impl_dir)

   return layer_impls


# Generates an AXI Stream input or output layer
# direction is either "in" or "out"
def gen_axi_io_layer(layer_spec, odir, direction):
   template_path = os.getenv('SCALE_CNN_ROOT') + "/templates/axi_{}/".format(direction)
   name = layer_spec['name']
   # Fill out some other fields for the layer
   chans = layer_spec['chans']
   layer_spec['chans_padded'] = 4 if chans == 3 else chans
   # AXI Stream side channel fields, default to 0 if unspecified.
   axi_fields = ["AXIS_WUser", "AXIS_WId", "AXIS_WDest"]
   for field in axi_fields:
      if field not in layer_spec:
         layer_spec[field] = 0
   # Finally, do the generation.
   layer_files = get_axi_io_layer_files(name, direction)
   gen_layer_files(layer_spec, layer_files, odir, template_path)
   full_layer_name = name + "_axi_" + direction
   print("Generated AXI4-Stream I/O layer {}".format(full_layer_name))


# Create a file with a list of implementation directories.
# It will be a text file where each line is a string representation of the 
# dictionary that describes each implementation.
def gen_layer_impl_list(odir, layer_spec, implementations):
   lname = layer_spec['layer_name']
   fp = os.path.abspath(os.path.join(odir, lname + "_implementations.txt"))
   # Delete these keys, we don't need them anymore
   del_keys = ['accum_functions', 'accum_function_calls']
   latencies = []
   with open(fp, 'w') as f:
      f.write(str(layer_spec) + "\n")
      for impl in implementations:
         latencies.append(impl['estimated_latency'])
         for k in del_keys:
            impl.pop(k, None)
         f.write(str(impl))
         f.write("\n")
   in_dims = "{}x{}x{}".format(layer_spec['input_height'], \
                               layer_spec['input_width'],  \
                               layer_spec['input_chans'])
   out_dims = "{}x{}x{}".format(layer_spec['output_height'], \
                                layer_spec['output_width'],  \
                                layer_spec['output_chans'])
   filter_dims = "{}x{}".format(layer_spec['filter_size'], layer_spec['filter_size'])
   if layer_spec['layer_type'] == 'conv-conv':
      intermediate_dims = "{}x{}x{}".format(layer_spec['output_height'], \
                                            layer_spec['output_width'],  \
                                            layer_spec['intermediate_chans'])
      dim_str = '{} -> {} -> {}'.format(in_dims, intermediate_dims, out_dims)
   else:
      dim_str = '{} -> {}'.format(in_dims, out_dims)
   print("{} ({}): {} ({} filters), {} implementations, estimated latency range {:,} to {:,} cycles".format( \
      lname, layer_spec['layer_type'], dim_str, filter_dims, len(implementations), min(latencies), max(latencies)))


# Given a path to a layer config file, generates all the files needed
# for that layer in the specified output directory.
def generate_layer(layer_spec, odir, args):
   if 'layer_name' in layer_spec:
      layer_spec['lname'] = layer_spec['layer_name'] # shorthand
   layer_type = layer_spec['layer_type']
   if not os.path.isdir(odir):
      os.makedirs(odir)
   if layer_type == 'conv' or layer_type == 'conv-max' or layer_type == 'conv-conv':
      implementations = gen_conv_layer(layer_spec, odir, args)
      gen_layer_impl_list(odir, layer_spec, implementations)
   elif layer_type == 'axi_in' or layer_type == 'axi_out':
      direction = layer_type[layer_type.index('_')+1:]
      gen_axi_io_layer(layer_spec, odir, direction)
   else:
      raise Exception('Unknown layer type: {}'.format(layer_type))


