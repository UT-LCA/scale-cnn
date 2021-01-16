# network_gen.py
# Code for generating files needed to synthesize entire networks.
import os
import layer_gen

def gen_network_layers(network_spec, odir, min_ii, max_ii):
   print("Generating layers for {} network.".format(network_spec['name']))
   layers = network_spec['layers']

   # Before generating all the layers, we might be able to speed things up by
   # increasing the minimum II to the largest minimum layer latency. This enables
   # us to eliminate certain design points for certain layers so we don't have to 
   # synthesize them.
   max_min_latency = -1
   for i, layer in enumerate(layers):
      # Copy common params to each layer (this is for brevity in the JSON)
      if 'common_params' in network_spec:
         layer.update(network_spec['common_params'])
      # Fill in the output dimensions for all layers except the last
      if i < len(layers) - 1:
         layer['output_height'] = layers[i+1]['input_height']
         layer['output_width']  = layers[i+1]['input_width']
         layer['output_chans']  = layers[i+1]['input_chans']
      # Give the layer a name
      layer['layer_name'] = network_spec['shorthand_name'] + str(i+1)
      options = layer_gen.GetLayerImplOptions(layer, min_ii, max_ii)
      min_l = min([l for (r, o, l) in options])
      if min_l > max_min_latency:
         max_min_latency = min_l

   if max_min_latency > min_ii:
      print("Adjusting minimum ii to {} cycles".format(max_min_latency))
      adjusted_min_ii = max_min_latency
   else:
      adjusted_min_ii = min_ii

   # Put all the layers under "layers" subdirectory

   # Generate the layers
   for layer in layers:
      layer_odir = os.path.join(odir, "layers/" + layer['layer_name'])
      layer_gen.generate_layer(layer, layer_odir, adjusted_min_ii, max_ii)

   print("Layer generation complete.")
