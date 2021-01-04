import os
import argparse
import layer_gen
import hls
import json

# Finds and reads the layer.json for the layer
def read_layer_spec(json_path):
   if not os.path.isfile(json_path):
      raise Exception('Error: could not find layer json file at {}.'.format(json_path))
   layer_spec = {}
   with open(json_path, 'r') as jf:
      layer_spec = json.load(jf)
   layer_spec['lname'] = layer_spec['layer_name'] # shorthand
   return layer_spec

if __name__ == '__main__':
   parser = argparse.ArgumentParser()
   parser.add_argument('mode') # positional
   parser.add_argument('-l', '--layerspec', help="Path to the layer specification")
   parser.add_argument('-i', '--input',     help="Path to input file (not layer or network)")
   parser.add_argument('-o', '--output',    help="Output directory")
   parser.add_argument('-ss', '--skip_synth', action='store_true', help="Skip synthesis, just analyze reports")
   args = parser.parse_args()

   mode  = args.mode

   layer_spec = {}
   if args.layerspec is not None:
      layer_spec = read_layer_spec(args.layerspec)

   if os.getenv('SCALE_CNN_ROOT') is None:
      raise Exception('SCALE_CNN_ROOT environment variable is not set!')

   if mode == 'gen_layer':
      layer_gen.generate_layer(layer_spec, args.output)
   elif mode == 'explore_layer':
      hls.explore_layer_implementations(layer_spec, args.input, args.skip_synth)
   else:
      print("Unrecognized mode argument.")
