import os
import argparse
import layer_gen


if __name__ == '__main__':
   parser = argparse.ArgumentParser()
   parser.add_argument('mode') # positional
   parser.add_argument('-p', '--path',   help="Path to the layer or network directory")
   parser.add_argument('-i', '--input',  help="Input layer/network config")
   parser.add_argument('-o', '--output', help="Output directory")
   args = parser.parse_args()

   mode  = args.mode

   if mode == 'genlayer':
      layer_gen.generate_layer(args.input, args.output)
   else:
      print("Unrecognized mode argument.")
