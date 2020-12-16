# layer_gen.py
# Contains the code for generating the files needed for a layer
# given its template and its specification
from functools import reduce
import copy
import os
import string

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
                 ('run.sh', 'run.sh'),
                 ('viewreport.sh', 'viewreport.sh')]
   return impl_files
   

# Given a template file, substitutions, and output file path,
# copies the template file to the output file making the appropriate substitutions.
# If it is a .sh file, it also makes it executable.
def make_file_from_template(template_fp, output_fp, substitutions):
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


# Generates a conv layer from the template, and generates the different implementations
# Returns a list of dicts that describe each implementation, including their directory.
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
      impl = {}
      impl['scale_factor'] = sf
      # Need to pick a name for the implementation
      # For now pick "sf#"
      impl['name'] = "sf{}".format(impl['scale_factor'])
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
   layer_spec['fpga_part'] = 'xcku115-flvf1924-3-e'
   if not os.path.isdir(odir):
      os.mkdir(odir)
   if layer_type == 'conv':
      implementations = gen_conv_layer(layer_spec, odir)
   else:
      raise Exception('Unknown layer type: {}'.format(layer_type))

   gen_layer_impl_list(odir, layer_spec, implementations)