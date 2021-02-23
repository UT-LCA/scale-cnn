# utils.py
# Shared code between different python modules
import os
import string
import json
import math

# Takes in a path to a JSON file and reads it then returns the object or list
def read_json(json_path):
   if not os.path.isfile(json_path):
      raise Exception('Error: could not find json file at {}.'.format(json_path))
   with open(json_path, 'r') as jf:
      return json.load(jf)

# Writes JSON to a filepath
def write_json(json_path, thing):
   with open(json_path, 'w') as f:
      json.dump(thing, f)

# Given a template file, substitutions, and output file path,
# copies the template file to the output file making the appropriate substitutions.
# If it is a .sh file, it also makes it executable.
def make_file_from_template(template_fp, output_fp, substitutions):
   output_fp_dir = os.path.dirname(output_fp)
   if not os.path.isdir(output_fp_dir):
      os.makedirs(output_fp_dir)
   with open(template_fp, 'r') as ifile, open(output_fp, 'w') as ofile:
      template = string.Template(ifile.read())
      ofile.write(template.substitute(substitutions))
   if output_fp.endswith('.sh'):
      os.system('chmod +x ' + output_fp)

# Separate a list of design points into Pareto and non-Pareto optimal points.
# points is the list of design points
# pareto_superior is a function that takes in two points and returns True if 
# the first point is Pareto-superior to the second point.
def pareto_sort(points, pareto_superior):
   pareto_points     = []
   non_pareto_points = []
   for new_point in points:

      pareto_optimal = True
      for p_point in pareto_points:
         if pareto_superior(p_point, new_point):
            # If an existing Pareto-optimal point is Pareto-superior to
            # the new point, then we can immediately place the new point
            # in the non-Pareto optimal points. And we are guaranteed that
            # the new point is not Pareto-superior to any other point currently
            # in pareto_points
            pareto_optimal = False
            break
         if pareto_superior(new_point, p_point):
            # If the new point is Pareto-superior to an existing point in pareto_points,
            # then the existing point is no longer considered Pareto-optimal. Move it
            # to non_pareto_points
            pareto_points.remove(p_point)
            non_pareto_points.append(p_point)

      # If there were no points that were Pareto-superior to the new point, then the 
      # new point is considered to be Pareto-optimal.
      if pareto_optimal:
         pareto_points.append(new_point)
      else:
         non_pareto_points.append(new_point)

   return (pareto_points, non_pareto_points)
            

# Calculates the number of URAM blocks required for either the input or output 
# feature maps of a certain layer in the network, specified by argument d ("input" or "output")
# This does not count any filters that are stored in URAMs.
def calc_num_urams(layer_spec, d):
   h = layer_spec['%s_height' % d]
   w = layer_spec['%s_width'  % d]
   c = layer_spec['%s_chans'  % d]
   # Round up chans to nearest multiple of 4
   c = 4*math.ceil(c/4)
   # There are a total of h*w*c 16-bit words to store, and we can fit four in a single
   # 72-bit URAM row. Calculate the number of rows we need
   uram_rows = h*w*c / 4
   # There are 4096 rows per URAM block.
   # We multiply by 2 because of the double-buffering that is required between stages
   # of a dataflow pipeline.
   return math.ceil(uram_rows / 4096) * 2


# Functions for calculating total number of filter words for a layer
def calc_num_l1_filter_words(layer):
   l1_ochans = layer['intermediate_chans'] if layer['layer_type'] == 'conv-conv' else layer['output_chans'] 
   return l1_ochans * (layer['filter_size']**2) * layer['input_chans']

def calc_num_l2_filter_words(layer):
   if layer['layer_type'] == 'conv-conv':
      return layer['intermediate_chans'] * layer['output_chans'] # assumes L2 is 1x1 filters
   else:
      return 0

def calc_num_filter_words(layer):
   return calc_num_l1_filter_words(layer) + calc_num_l2_filter_words(layer)

# Given a layer specification, returns the number of URAMs and BRAMs that would
# be required to implement the filters for that layer. It calculates this based
# on whether the layer is currently configured to use BRAMs or URAMs for the
# filter data.
def calc_filter_rams(layer):
   filter_urams = 0
   filter_brams = 0
   l1_words = calc_num_l1_filter_words(layer)
   l2_words = calc_num_l2_filter_words(layer)
   # BRAMs are 1K rows * 18 bits. Each word is 16 bits. The other two bits are unused,
   # so each BRAM can hold 1024 words
   words_per_bram = 1024
   # URAMs are 4K rows * 72 bits. We reshape the array so each row holds 4 words.
   # Therefore each URAM can hold 4*4K words
   words_per_uram = 4*4096
   if layer['filter_ram_type'] == 'bram':
      filter_brams += math.ceil(l1_words / words_per_bram)
   else:
      filter_urams += math.ceil(l1_words / words_per_uram)
   # For conv-conv layers, the L2 filters are alway stored in BRAMs.
   filter_brams += math.ceil(l2_words / words_per_bram)
   return filter_urams, filter_brams

# Returns a list of keys that should be in every top-level network or layer spec.
def get_top_level_keys():
   return ['fpga_part', 'target_clock_period', 'hadd_latency', 'hmul_latency', 'reduce_dsp_usage']

# Sets default values if unspecified for the layer or network (depending on which is the "top level")
def set_spec_defaults(spec):
   defaults = {
      'target_clock_period': 10, # ns
      'hadd_latency': 1, # cycles
      'hmul_latency': 1, # cycles
      'reduce_dsp_usage': False
   }
   for k in defaults.keys():
      if k not in spec:
         spec[k] = defaults[k]

# Loads FPGA info
def get_fpga_info():
   fpga_json_fp = os.path.join(os.getenv('SCALE_CNN_ROOT'), 'fpgas', 'fpgas.json')
   return read_json(fpga_json_fp)

