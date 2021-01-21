# network_gen.py
# Code for generating files needed to synthesize entire networks.
import os
import layer_gen
import utils
import hls

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

def gen_network_layers(network_spec, odir, args):
   print("Generating layers for {} network.".format(network_spec['name']))
   complete_layer_specs(network_spec)
   layers = network_spec['layers']
   # Before generating all the layers, we might be able to speed things up by
   # increasing the minimum II to the largest minimum layer latency. This enables
   # us to eliminate certain design points for certain layers so we don't have to 
   # synthesize them.
   max_min_latency = -1
   for layer in layers:
      options = layer_gen.GetLayerImplOptions(layer, args.min_ii, args.max_ii)
      min_l = min([l for (r, o, l) in options])
      if min_l > max_min_latency:
         max_min_latency = min_l

   if max_min_latency > args.min_ii:
      print("Adjusting minimum ii to {} cycles".format(max_min_latency))
      args.min_ii = max_min_latency

   # Put all the layers under "layers" subdirectory
   for layer in layers:
      layer_odir = os.path.join(odir, "layers/" + layer['layer_name'])
      layer_gen.generate_layer(layer, layer_odir, args)

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


# Compare function for design points to decide which are Pareto-optimal and which are not.
# It takes in two design points, a and b, that have 'cost' and 'cycles' attributes.
# It returns True if point A has lower cost and lower cycles than point B. Otherwise,
# it returns False.
def cost_perf_compare(a, b):
   return a['cost'] <= b['cost'] and a['cycles'] <= b['cycles']

# Analyzes the synthesized layer results and outputs a list of intelligently-chosen
# design points of different layer implementations to get overall network implementations
# that have a cost/performance trade-off.
def analyze_network_options(network_spec, network_root_dir, args):
   print("Analyzing layer synthesis results to create network implementation options.\n")
   complete_layer_specs(network_spec)
   layers = network_spec['layers']

   # layer_implems is a dict of lists of dicts where each list pertains to 
   # one layer and each dict pertains to one Pareto-optimal implementation of it.
   # The keys of layer_impls are the layer names.
   layer_impls = {}
   for layer in layers:
      lname = layer['layer_name']
      print("Layer: {}".format(lname))
      impl_list_path = os.path.join(network_root_dir, 'layers', lname, lname + str('_implementations.txt'))
      layer_spec, implementations = hls.read_layer_implementations(impl_list_path)
      print("{} implementations".format(len(implementations)))
      layer_options = []
      for impl in implementations:
         report_info = hls.analyze_reports(layer_spec, impl, args)
         layer_options.append({'name'  : impl['name'], \
                               'cost'  : report_info['cost_info']['total'], \
                               'cycles': report_info['true_latency']})
      # Separate the layer design points into Pareto and non-Pareto optimal points.
      pareto_points, non_pareto_points = utils.pareto_sort(layer_options, cost_perf_compare)
      # Sort the layer options by execution time, highest to lowest.
      pareto_points.sort(key=lambda p: p['cycles'])
      pareto_points.reverse()
      non_pareto_points.sort(key=lambda p: p['cycles'])
      non_pareto_points.reverse()
      print("Pareto-optimal ({}):".format(len(pareto_points)))
      for p in pareto_points:
         print("{}: cost: {:.4f}, cycles: {:,}".format(p['name'], p['cost'], p['cycles']))
      print("\nNot Pareto-optimal ({}):".format(len(non_pareto_points)))
      for p in non_pareto_points:
         print("{}: cost: {:.4f}, cycles: {:,}".format(p['name'], p['cost'], p['cycles']))
      print('\n')
      # When choosing combinations of layer implementations to form entire networks,
      # we only want to consider the points that are Pareto-optimal.
      layer_impls[lname] = pareto_points

   # Now we must put all the layer implementation options together to come up with network
   # implementation options. Each list is a list of options for one layer from slowest to fastest.
   # The algorithm is as follows:
   # 1. Pick the slowest remaining implementation option of each layer. 
   #    This becomes a new design point for the network.
   # 2. Find the maximum latency of all the layers in this design point. 
   #    This value (+1) will be the II of the network pipeline.
   # 3. For each list whose slowest design point is exactly equal to that value,
   #    remove that design point from the list.
   # 4. Repeat until there is a layer whose list of design points is now empty.
   print('\nGenerating network implementation options...')
   network_implementations = []
   stop = False
   while not stop:
      network_impl = {}
      max_latency = -1
      for l in layer_impls.keys():
         network_impl[l] = layer_impls[l][0]
         cycles = layer_impls[l][0]['cycles']
         max_latency = max(max_latency, cycles)
      network_impl['max_latency'] = max_latency
      for l in layer_impls.keys():
         if layer_impls[l][0]['cycles'] == max_latency:
            layer_impls[l].pop(0)
            if len(layer_impls[l]) == 0:
               stop = True
      # TODO finish this, just print the options right now.
      print(str(network_impl))

