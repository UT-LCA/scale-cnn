# layer_gen.py
# Contains the code for generating the files needed for a layer
# given its template and its specification
from functools import reduce
import copy
import os
import string
import accum
import math

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
   
# Given a template file, substitutions, and output file path,
# copies the template file to the output file making the appropriate substitutions.
# If it is a .sh file, it also makes it executable.
def make_file_from_template(template_fp, output_fp, substitutions):
   output_fp_dir = os.path.dirname(output_fp)
   if not os.path.isdir(output_fp_dir):
      os.mkdir(output_fp_dir)
   with open(template_fp, 'r') as ifile, open(output_fp, 'w') as ofile:
      template = string.Template(ifile.read())
      ofile.write(template.substitute(substitutions))
   if output_fp.endswith('.sh'):
      os.system('chmod +x ' + output_fp)

def gen_layer_files(layer_spec, layer_files, odir, template_path):
   for layer_file, template_fname in layer_files:
      template_fp = os.path.join(template_path, template_fname)
      output_fp = os.path.join(odir, layer_file)
      make_file_from_template(template_fp, output_fp, layer_spec)

def gen_layer_impl_files(layer_spec, impl_files, impl, odir, template_path):
   impl_dir = os.path.join(odir, impl['name'])
   if not os.path.isdir(impl_dir):
      os.mkdir(impl_dir)
   for impl_file, template_fname in impl_files:
      template_fp = os.path.join(template_path, 'impl', template_fname)
      output_fp = os.path.join(odir, impl['name'], impl_file)
      substitutions = copy.copy(layer_spec)
      substitutions.update(impl)
      make_file_from_template(template_fp, output_fp, substitutions)
   return impl_dir

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
   OH = layer_spec['output_height']
   OW = layer_spec['output_width']
   OC = layer_spec['output_chans']
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
   # I will estimate that the latency is II*3
   pipeline_depth = II*3
   return II*top_loop_iters + pipeline_depth

# To avoid using an ungodly amount of resources, we must put a reasonable limit on how
# much scaling we can allow in a single layer.
# For right now, choosing 200 as the maximum.
MAX_TOTAL_SCALE_FACTOR = 200

# Return a list of all possible configurations for a conv layer
# It eliminates options that either have too large total scaling, or are outside
# a specified range of latencies.
# min and max latencies are in cycles, -1 if no limit.
# Each element is (read_sf, ochan_sf, estimated_latency)
def get_conv_impl_options(layer_spec, min_latency, max_latency):
   # Different implementations for conv layers:
   # - Read Scale factor: Factors of input chans
   # - Output channel scale factor: Factors of output chans
   ichans_factors = factors(layer_spec['input_chans'])
   ochans_factors = factors(layer_spec['output_chans'])
   # Sort and remove duplicates
   read_scale_factors  = sorted(list(set(ichans_factors)))
   ochan_scale_factors = sorted(list(set(ochans_factors)))
   options = []
   for read_sf in read_scale_factors:
      for ochan_sf in ochan_scale_factors:
         # Make sure the total scaling is less than the max.
         total_scale = read_sf * ochan_sf
         if total_scale > MAX_TOTAL_SCALE_FACTOR:
            continue
         # Estimate the latency given the scale factors.
         est_lat = GetConvEstimatedLatency(layer_spec, read_sf, ochan_sf)
         if (min_latency != -1 and est_lat < min_latency) or \
            (max_latency != -1 and est_lat > max_latency):
            continue
         options.append((read_sf, ochan_sf, est_lat))
   return options
   

# Generates a conv layer from the template, and generates the different implementations
# Returns a list of dicts that describe each implementation, including their directory.
def gen_conv_layer(layer_spec, odir, min_latency, max_latency):
   global MAX_TOTAL_SCALE_FACTOR
   layer_type = layer_spec['layer_type']
   template_path = os.getenv('SCALE_CNN_ROOT') + "/templates/{}/".format(layer_type)

   # Determine input and output words per URAM row.
   ichans = layer_spec['input_chans']
   ochans = layer_spec['output_chans']
   layer_spec['input_words_per_uram_row']  = 4   # Currently just always 4 since we pad to 4 if there are 3.
   layer_spec['output_words_per_uram_row'] = GetUramWordsPerRow(ochans)     
   layer_spec['input_chans_padded'] = 4 * math.ceil(ichans / 4)

   # Generate the layer-specific files once
   layer_name  = layer_spec['layer_name']
   layer_files = get_conv_layer_files(layer_name)
   impl_files  = get_conv_layer_impl_files(layer_name, layer_type)
   gen_layer_files(layer_spec, layer_files, odir, template_path)
   
   # Generate all of the possible conv implementations
   impl_options = get_conv_impl_options(layer_spec, min_latency, max_latency)
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
                                       ochan_sf = ochan_sf,
                                       target_latency=dp_latency, 
                                       input_length=vec_size,
                                       read_bw = read_sf*2 )

      impl['accum_functions']      = accum_funcs
      impl['accum_function_calls'] = accum_func_calls

      # Pick a name for the implementation
      impl['name'] = "r{}_o{}".format(read_sf, ochan_sf)
      layer_impls.append(impl)

   # Now, generate the files for each implementation
   for impl in layer_impls:
      impl_dir = gen_layer_impl_files(layer_spec, impl_files, impl, odir, template_path)
      impl['dir'] = os.path.abspath(impl_dir)

   return layer_impls


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
   print("{} ({}): {} -> {}, {} implementations, estimated latency range {:,} to {:,} cycles".format( \
      lname, layer_spec['layer_type'], in_dims, out_dims, len(implementations), min(latencies), max(latencies)))


# Given a path to a layer config file, generates all the files needed
# for that layer in the specified output directory.
def generate_layer(layer_spec, odir, min_latency, max_latency):
   layer_spec['lname'] = layer_spec['layer_name'] # shorthand
   layer_type = layer_spec['layer_type']
   # TODO: Enable different FPGAs. For now, always use this one (Kintex 7 Ultrascale+)
   layer_spec['fpga_part'] = 'xcku11p-ffva1156-2-e'
   if not os.path.isdir(odir):
      os.mkdir(odir)
   if layer_type == 'conv' or layer_type == 'conv-max':
      implementations = gen_conv_layer(layer_spec, odir, min_latency, max_latency)
   else:
      raise Exception('Unknown layer type: {}'.format(layer_type))

   gen_layer_impl_list(odir, layer_spec, implementations)

