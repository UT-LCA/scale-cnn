import os
import argparse
import layer_gen
import network_gen
import hls
import utils

if __name__ == '__main__':
   parser = argparse.ArgumentParser()
   mode_options = ['gen_layer', 'explore_layer', 'gen_network_layers', 'synth_network_layers', \
                   'gen_network_implementations', 'analyze_network_options']
   parser.add_argument('mode', choices=mode_options) # positional
   parser.add_argument('-l', '--layerspec', help="Path to a layer specification")
   parser.add_argument('-n', '--networkspec', help="Path to a network specification")
   parser.add_argument('-i', '--input', help="Path to input file / directory (not layer or network spec)")
   parser.add_argument('-o', '--output', help="Output directory")
   parser.add_argument('-ss', '--skip_synth', action='store_true', help="Skip synthesis, just analyze reports")
   parser.add_argument('-fs', '--fast_compile', action='store_true', help="Reduce the number of top-level loop iterations to reduce synth time.")
   parser.add_argument('-cf', '--cost_function', choices=['default', 'no_luts', 'dsp_only'], default='default', help="Cost function")
   parser.add_argument('--min_ii', help="Minimum II for network dataflow pipeline / latency of one layer")
   parser.add_argument('--max_ii', help="Maximum II for network dataflow pipeline / latency of one layer")
   parser.add_argument('--network_options', type=int, default=10, help="Maximum number of network options to generate")
   parser.add_argument('--impl', help="Specify a specific network implementation to generate. Generates all if unspecified.")
   args = parser.parse_args()

   mode  = args.mode

   args.min_ii = -1 if args.min_ii is None else args.min_ii
   args.max_ii = -1 if args.max_ii is None else args.max_ii

   spec = {}
   if args.layerspec is not None:
      spec = utils.read_json(args.layerspec)
   if args.networkspec is not None:
      spec = utils.read_json(args.networkspec)

   if os.getenv('SCALE_CNN_ROOT') is None:
      raise Exception('SCALE_CNN_ROOT environment variable is not set!')

   if mode == 'gen_layer':
      layer_gen.generate_layer(spec, args.output, args)
   elif mode == 'explore_layer':
      hls.explore_layer_implementations(args.input, args)
   elif mode == 'gen_network_layers':
      network_gen.gen_network_layers(spec, args.output, args)
   elif mode == 'synth_network_layers':
      hls.synth_network_layers(spec, args.input)
   elif mode == 'gen_network_implementations':
      network_gen.gen_network_implementations(spec, args.input, args)
   elif mode == 'analyze_network_options':
      network_gen.analyze_network_options(spec, args.input, args)
   else:
      print("Unrecognized mode argument.")

