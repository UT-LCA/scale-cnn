# layer_gen.py
# Contains the code for generating the files needed for a layer
# given its template and its specification
from functools import reduce
import copy
import os
import string
import accum
import math

def get_layer_files(layer_name):
   # Each tuple is ([name of file to be created], [name of template file])
   # List of once-per-layer files
   layer_files = [('global_defines.h', 'global_defines.h'), \
                  ('{}_common_defines.h'.format(layer_name), 'common_defines.h'), \
                  ('{}_common_directives.tcl'.format(layer_name), 'common_directives.tcl'), \
                  ('test/tb_{}.cpp'.format(layer_name), 'test/testbench.cpp'), \
                  ('test/golden.cpp', 'test/golden.cpp')]

   return layer_files

def get_layer_impl_files(layer_name, layer_type):
   # List of once-per-layer-implementation files
   impl_files = [('{}.cpp'.format(layer_name), '{}.cpp'.format(layer_type)), \
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

def gen_layer_files(layer_spec, odir, template_path):
   layer_name = layer_spec['layer_name']
   layer_files = get_layer_files(layer_name)
   for layer_file, template_fname in layer_files:
      template_fp = os.path.join(template_path, template_fname)
      output_fp = os.path.join(odir, layer_file)
      make_file_from_template(template_fp, output_fp, layer_spec)

def gen_layer_impl_files(layer_spec, impl, odir, template_path):
   impl_dir = os.path.join(odir, impl['name'])
   if not os.path.isdir(impl_dir):
      os.mkdir(impl_dir)
   layer_name = layer_spec['layer_name']
   layer_type = layer_spec['layer_type']
   impl_files = get_layer_impl_files(layer_name, layer_type)
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


# Generates a conv layer from the template, and generates the different implementations
# Returns a list of dicts that describe each implementation, including their directory.
def gen_conv_layer(layer_spec, odir):
   template_path = os.getenv('SCALE_CNN_ROOT') + "/templates/conv/"

   # Determine input and output words per URAM row.
   ichans = layer_spec['input_chans']
   ochans = layer_spec['output_chans']
   layer_spec['input_words_per_uram_row']  = GetUramWordsPerRow(ichans)
   layer_spec['output_words_per_uram_row'] = GetUramWordsPerRow(ochans)

   # Generate the layer-specific files once
   gen_layer_files(layer_spec, odir, template_path)
   
   # Different implementations for conv layers:
   # - Read Scale factor:
   #   - Right now only choosing factors of input chans. Having difficulty getting
   #     ideally-scheduled BRAM reads beyond that. The padding pixel checks for
   #     readInputs makes it difficult for the synthesizer to schedule all the reads
   #     in parallel.
   # - Dot Product scale factor:
   #     - Equal to read scale factor
   #     - Equal to twice the read scale factor
   filter_size = layer_spec['filter_size']
   ichans_factors = factors(ichans)
   fsize_factors  = factors(filter_size)
   read_scale_factors = copy.copy(ichans_factors)
   #for f in fsize_factors:
   #  read_scale_factors.append(ichans*f)
   # Sort and remove duplicates
   read_scale_factors = list(set(read_scale_factors))
   read_scale_factors.sort()

   # Generate all of the possible conv implementations
   layer_impls = []
   for read_sf in read_scale_factors:
      for double_dp_sf in [False, True]:
         impl = {}
         dp_sf = 2*read_sf if double_dp_sf else read_sf
         impl['read_scale_factor'] = read_sf
         impl['dp_scale_factor']   = dp_sf

         # Generate the custom code for the accumulation functions for this layer.
         # Target latency is the estimated latency of the readInputs stage.
         vec_size = (layer_spec['filter_size'] ** 2) * layer_spec['input_chans']
         rdInp_latency = math.ceil(vec_size / read_sf) + 3
         accum_funcs, accum_func_calls = accum.GenerateAccumulationStages( \
                                          layer_name = layer_spec['layer_name'],
                                          target_latency=rdInp_latency, 
                                          input_length=vec_size,
                                          read_bw = dp_sf )
         impl['accum_functions']      = accum_funcs
         impl['accum_function_calls'] = accum_func_calls

         # Pick a name for the implementation
         impl['name'] = "sf{}_{}".format(read_sf, dp_sf)
         layer_impls.append(impl)

   # Now, generate the files for each implementation
   for impl in layer_impls:
      impl_dir = gen_layer_impl_files(layer_spec, impl, odir, template_path)
      impl['dir'] = os.path.abspath(impl_dir)

   return layer_impls


# Create a file with a list of implementation directories.
# It will be a text file where each line is a string representation of the 
# dictionary that describes each implementation.
def gen_layer_impl_list(odir, layer_spec, implementations):
   lname = layer_spec['layer_name']
   fp = os.path.abspath(os.path.join(odir, lname + "_implementations.txt"))
   with open(fp, 'w') as f:
      for impl in implementations:
         f.write(str(impl))
         f.write("\n")
   print("Generated {} implementations for {}".format(len(implementations), lname))
   print("Generated implementation list at {}".format(fp))


# Given a path to a layer config file, generates all the files needed
# for that layer in the specified output directory.
def generate_layer(layer_spec, odir):
   layer_type = layer_spec['layer_type']
   # TODO: Enable different FPGAs. For now, always use this one (Kintex 7 Ultrascale+)
   layer_spec['fpga_part'] = 'xcku11p-ffva1156-2-e'
   if not os.path.isdir(odir):
      os.mkdir(odir)
   if layer_type == 'conv':
      implementations = gen_conv_layer(layer_spec, odir)
   else:
      raise Exception('Unknown layer type: {}'.format(layer_type))

   gen_layer_impl_list(odir, layer_spec, implementations)
