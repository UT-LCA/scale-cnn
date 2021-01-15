# network_gen.py
# Code for generating files needed to synthesize entire networks.
import os
import layer_gen

def gen_network_layers(network_spec, odir, min_ii, max_ii):
   print("Generating layers for {} network.".format(network_spec['name']))
   layers = network_spec['layers']
   for i, layer in enumerate(layers):
      # Fill in the output dimensions for all layers except the last
      if i < len(layers) - 1:
         layer['output_height'] = layers[i+1]['input_height']
         layer['output_width']  = layers[i+1]['input_width']
         layer['output_chans']  = layers[i+1]['input_chans']
      # Give the layer a name
      layer['layer_name'] = network_spec['shorthand_name'] + str(i+1)
      # Generate the layer
      layer_gen.generate_layer(layer_spec, odir, min_ii, max_ii)
   print("Layer generation complete.")
