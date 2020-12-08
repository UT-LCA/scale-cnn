# layer_gen.py
# Contains the code for generating the files needed for a layer
# given its template and its specification
import json

# Finds and reads the layer.json for the layer
def read_layer_spec(json_path):
   if not os.path.isfile(json_path):
      raise Exception('Error: could not find layer json file at {}.'.format(json_path))
   layer_spec = {}
   with open(json_path, 'r') as jf:
      layer_spec = json.load(jf)
   return layer_spec

# Given a path to a layer config file, generates all the files needed
# for that layer in the specified output directory.
def generate_layer(idir, odir):
   # Read in the layer spec
   layer_spec = read_layer_spec(idir)
   layer_type = layer_spec['layer_type']
   if layer_type == 'conv':
      gen_conv_layer(layer_spec, odir)
   else:
      raise Exception('Unknown layer type: {}'.format(layer_type))
