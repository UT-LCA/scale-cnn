# network_gen.py
# Code for generating files needed to synthesize entire networks.
import os
import layer_gen
import utils

def complete_layer_specs(network_spec):
   layers = network_spec['layers']
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
      layer['lname'] = layer['layer_name']

def gen_network_layers(network_spec, odir, min_ii, max_ii):
   print("Generating layers for {} network.".format(network_spec['name']))
   complete_layer_specs(network_spec)
   layers = network_spec['layers']
   # Before generating all the layers, we might be able to speed things up by
   # increasing the minimum II to the largest minimum layer latency. This enables
   # us to eliminate certain design points for certain layers so we don't have to 
   # synthesize them.
   max_min_latency = -1
   for layer in layers:
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
   for layer in layers:
      layer_odir = os.path.join(odir, "layers/" + layer['layer_name'])
      layer_gen.generate_layer(layer, layer_odir, adjusted_min_ii, max_ii)

   print("Layer generation complete.")


# Generates the text substitutions necessary for a network implementation
def get_network_substitutions(network_spec, layer_impls):
   s = ' ' * 3
   layers = network_spec['layers']
   fmap_decls   = ''
   filter_decls = ''
   layer_calls  = ''
   layer_decls  = ''
   layer_dicts  = ''
   for i, l in enumerate(layers):
      name = l['layer_name']
      ih = l['input_height']
      iw = l['input_width']
      ic = l['input_chans']
      icp = 4 if ic == 3 else ic
      oh = l['output_height']
      ow = l['output_width']
      oc = l['output_chans']
      fs = l['filter_size']
      last = (i == len(layers) - 1)
      ifmaps  = '{}_fmaps'.format(name)
      ofmaps  = 'final_fmaps' if last else '{}_fmaps'.format(layers[i+1]['layer_name'])
      filters = '{}_filters'.format(name)
      in_dims   = '[{}][{}][{}]'.format(ih, iw, icp)
      out_dims  = '[{}][{}][{}]'.format(oh, ow, oc)
      filt_dims = '[{}][{}][{}][{}]'.format(oc, fs, fs, ic)
      fmap_decls   += s + 'data_t {}{};\n'.format(ifmaps, in_dims)
      filter_decls += s + 'data_t {}{};\n'.format(filters, filt_dims)
      layer_calls  += s + '{}({}, {}, {});\n'.format(name, ifmaps, ofmaps, filters)
      # Header declaration for each layer
      layer_decls += 'void {}(\n  data_t in_data{},\n  data_t out_data{},\n  data_t filter_data{});\n\n'\
         .format(name, in_dims, out_dims, filt_dims)
      # TCL declaration of path to layer implementation
      layer_dicts += s + '[dict create name {} path {}] \\\n'.format(name, layer_impls[name])

   # Final feature maps
   fmap_decls += s + 'data_t final_fmaps[{}][{}][{}];\n'.format( \
      layers[-1]['output_height'], layers[-1]['output_width'], layers[-1]['output_chans'])
   
   # TODO: Enable different FPGAs. For now, always use this one (Kintex 7 Ultrascale+)
   substitutions = {
      "fpga_part"           : 'xcku11p-ffva1156-2-e',
      "name"                : network_spec['name'],
      "fmap_declarations"   : fmap_decls,
      "filter_declarations" : filter_decls,
      "layer_calls"         : layer_calls,
      "layer_declarations"  : layer_decls,
      "layer_dicts"         : layer_dicts
   }
   return substitutions


# Generates one implementation of a network
# layer_impls is a dictionary whose keys are layer names and values
# are the paths to the implementation.
def gen_network_impl(network_spec, odir, impl_name, layer_impls):
   substitutions = get_network_substitutions(network_spec, layer_impls)
   template_path = os.path.join(os.getenv('SCALE_CNN_ROOT'), 'templates/network')
   template_files = ['network.cpp', 'network_layers.h', 'network_layers.tcl', 'network.tcl']
   for tf in template_files:
      template_fp = os.path.join(template_path, tf)
      output_fp   = os.path.join(odir, impl_name, tf.replace('network', network_spec['name']))
      utils.make_file_from_template(template_fp, output_fp, substitutions)


# Generates multiple implementations of a network
# For right now this is a "test version" that only picks the cheapest option of each layer.
def gen_network_implementations(network_spec, network_root_dir):
   print("Generating one implementation for the network")
   complete_layer_specs(network_spec)
   impl_name = "im1"
   layer_impls = {}
   network_root_abs = os.path.abspath(network_root_dir)
   for layer in network_spec['layers']:
      lname = layer['layer_name']
      layer_impls[lname] = os.path.join(network_root_abs, 'layers', lname, 'r1_o1')
   impl_top_dir = os.path.join(network_root_abs, 'impls')
   gen_network_impl(network_spec, impl_top_dir, impl_name, layer_impls)
   print("Generation complete.")

   
