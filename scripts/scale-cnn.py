import os
import argparse
import layer_gen
import network_gen
import hls
import json

# Finds and reads layer or netowrk json
def read_json_spec(json_path):
   if not os.path.isfile(json_path):
      raise Exception('Error: could not find json file at {}.'.format(json_path))
   spec = {}
   with open(json_path, 'r') as jf:
      spec = json.load(jf)
   return spec

if __name__ == '__main__':
   parser = argparse.ArgumentParser()
   mode_options = ['gen_layer', 'explore_layer', 'gen_network_layers', 'synth_network_layers', \
                   'gen_network_implementations']
   parser.add_argument('mode', choices=mode_options) # positional
   parser.add_argument('-l', '--layerspec', help="Path to a layer specification")
   parser.add_argument('-n', '--networkspec', help="Path to a network specification")
   parser.add_argument('-i', '--input', help="Path to input file / directory (not layer or network spec)")
   parser.add_argument('-o', '--output', help="Output directory")
   parser.add_argument('-ss', '--skip_synth', action='store_true', help="Skip synthesis, just analyze reports")
   parser.add_argument('-cf', '--cost_function', choices=['default', 'no_luts', 'dsp_only'], default='default', help="Cost function")
   parser.add_argument('--min_ii', help="Minimum II for network dataflow pipeline / latency of one layer")
   parser.add_argument('--max_ii', help="Maximum II for network dataflow pipeline / latency of one layer")
   args = parser.parse_args()

   mode  = args.mode

   min_ii = -1 if args.min_ii is None else args.min_ii
   max_ii = -1 if args.max_ii is None else args.max_ii

   spec = {}
   if args.layerspec is not None:
      spec = read_json_spec(args.layerspec)
   if args.networkspec is not None:
      spec = read_json_spec(args.networkspec)

   if os.getenv('SCALE_CNN_ROOT') is None:
      raise Exception('SCALE_CNN_ROOT environment variable is not set!')

   if mode == 'gen_layer':
      layer_gen.generate_layer(spec, args.output, -1, -1)
   elif mode == 'explore_layer':
      hls.explore_layer_implementations(spec, args.input, args.cost_function, args.skip_synth)
   elif mode == 'gen_network_layers':
      network_gen.gen_network_layers(spec, args.output, min_ii, max_ii)
   elif mode == 'synth_network_layers':
      hls.synth_network_layers(spec, args.input)
   elif mode == 'gen_network_implementations':
      network_gen.gen_network_implementations(spec, args.output)
   else:
      print("Unrecognized mode argument.")

