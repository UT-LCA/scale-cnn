# layer_gen.py
# Contains the code for generating the files needed for a layer
# given its template and its specification
from functools import reduce
import copy
import json
import os
import string

# Finds and reads the layer.json for the layer
def read_layer_spec(json_path):
   if not os.path.isfile(json_path):
      raise Exception('Error: could not find layer json file at {}.'.format(json_path))
   layer_spec = {}
   with open(json_path, 'r') as jf:
      layer_spec = json.load(jf)
   layer_spec['lname'] = layer_spec['layer_name'] # shorthand
   return layer_spec


def get_layer_files(layer_name):
   # Each tuple is ([name of file to be created], [name of template file])
   # List of once-per-layer files
   layer_files = [('global_defines.h', 'global_defines.h'), \
                  ('{}_common_defines.h'.format(layer_name), 'common_defines.h'), \
                  ('{}_common_directives.tcl'.format(layer_name), 'common_directives.tcl')]
   return layer_files

def get_layer_impl_files(layer_name, layer_type):
   # List of once-per-layer-implementation files
   impl_files = [('{}.c'.format(layer_name), '{}.c'.format(layer_type)), \
                 ('{}.tcl'.format(layer_name), '{}.tcl'.format(layer_type)), \
                 ('{}_impl_defines.h'.format(layer_name), 'impl_defines.h'), \
                 ('{}_impl_directives.tcl'.format(layer_name), 'impl_directives.tcl'), \
                 ('run.sh', 'run.sh')]
   return impl_files
   

def make_file_from_template(template_fp, output_fp, substitutions):
   with open(template_fp, 'r') as ifile, open(output_fp, 'w') as ofile:
      template = string.Template(ifile.read())
      ofile.write(template.substitute(substitutions))


def gen_layer_files(layer_spec, odir, template_path):
   layer_name = layer_spec['layer_name']
   layer_files = get_layer_files(layer_name)
   for layer_file, template_fname in layer_files:
      template_fp = os.path.join(template_path, template_fname)
      output_fp = os.path.join(odir, layer_file)
      make_file_from_template(template_fp, output_fp, layer_spec)


def gen_layer_impl_files(impl, odir, impl_name, template_path):
   impl_dir = os.path.join(odir, impl_name)
   if not os.path.isdir(impl_dir):
      os.mkdir(impl_dir)
   layer_name = impl['layer_name']
   layer_type = impl['layer_type']
   impl_files = get_layer_impl_files(layer_name, layer_type)
   for impl_file, template_fname in impl_files:
      template_fp = os.path.join(template_path, 'impl', template_fname)
      output_fp = os.path.join(odir, impl_name, impl_file)
      make_file_from_template(template_fp, output_fp, impl)


# Return a sorted list of all the integer factors of a positive integer
# Used to determine options for unroll factors.
def factors(n):
   x = list(set(reduce(list.__add__, 
               ([i, n//i] for i in range(1, int(n**0.5) + 1) if n % i == 0))))
   x.sort()
   return x


# Generates a conv layer from the template, and generates the different implementations
def gen_conv_layer(layer_spec, odir):
   template_path = os.getenv('SCALE_CNN_ROOT') + "/templates/conv/"
   # Generate the layer-specific files once
   gen_layer_files(layer_spec, odir, template_path)

   # Different implementations for conv layers:
   # - Scale factor:
   #   - First choose the factors of input channels
   #   - Then iterate over the factors of filter size, multiplying each by # input channels
   #     (this is when the scale factor goes beyond the number of input channels
   #   - Do not scale beyond that as at that point we might as well just do complete partitioning
   #     with registers.
   ichans      = layer_spec['input_chans']
   filter_size = layer_spec['filter_size']
   ichans_factors = factors(ichans)
   fsize_factors  = factors(filter_size)
   scale_factors = copy.copy(ichans_factors)
   for f in fsize_factors:
      scale_factors.append(ichans*f)
   # Sort and remove duplicates
   scale_factors = list(set(scale_factors))
   scale_factors.sort()

   # Generate all of the conv implementations
   # Right now it's just the scale factors.
   layer_impls = []
   for sf in scale_factors:
      impl = copy.copy(layer_spec)
      impl['scale_factor'] = sf
      layer_impls.append(impl)

   # Now, generate the files for each implementation
   for impl in layer_impls:
      # Need to pick a name for the implementation
      # For now pick "sf#"
      impl_name = "sf{}".format(impl['scale_factor'])
      gen_layer_impl_files(impl, odir, impl_name, template_path)


# Given a path to a layer config file, generates all the files needed
# for that layer in the specified output directory.
def generate_layer(ipath, odir):
   # Read in the layer spec
   layer_spec = read_layer_spec(ipath)
   layer_type = layer_spec['layer_type']
   # TODO: Enable different FPGAs. For now, always use this one (Kintex 7 Ultrascale+)
   layer_spec['fpga_part'] = 'xcku115-flvf1924-3-e'
   if not os.path.isdir(odir):
      os.mkdir(odir)
   if layer_type == 'conv':
      gen_conv_layer(layer_spec, odir)
   else:
      raise Exception('Unknown layer type: {}'.format(layer_type))
