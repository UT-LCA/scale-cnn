# network_gen.py
# Code for generating files needed to synthesize entire networks.
import os
import layer_gen
import utils
import hls
import copy
import math

def complete_layer_specs(network_spec):
   layers = network_spec['layers']
   keys = utils.get_top_level_keys()
   for i, layer in enumerate(layers):
      # Copy inherited params (this is for JSON brevity)
      if 'inherit' in layer:
         layer.update(network_spec[layer['inherit']]) 
         layer.pop('inherit', None)
      # Fill in the output dimensions for all layers except the last
      if i < len(layers) - 1:
         layer['output_height'] = layers[i+1]['input_height']
         layer['output_width']  = layers[i+1]['input_width']
         layer['output_chans']  = layers[i+1]['input_chans']
      # Give the layer a name
      layer['layer_name'] = network_spec['shorthand_name'] + str(i+1)
      layer['lname'] = layer['layer_name']
      # Copy top-levl keys from network spec to layer spec
      for k in keys:
         layer[k] = network_spec[k]

# This function is called before layer implementations for a network are generated.
# It checks that the specified FPGA has enough UltraRAMs on it to hold all of the 
# feature maps for different layers.
def check_uram_requirements(network_spec):
   print("Checking UltraRAM requirements are satisfied.")
   # Read the metadata for the FPGAs known to the tool.
   fpga_json_fp = os.path.join(os.getenv('SCALE_CNN_ROOT'), 'fpgas', 'fpgas.json')
   fpga_info = utils.read_json(fpga_json_fp)
   # The fpga_part either is a single string, or a list of strings for the 
   # case where multiple FPGAs are used.
   if isinstance(network_spec['fpga_part'], str):
      fpgas = [network_spec['fpga_part']]
   else:
      fpgas = network_spec['fpga_part']
   fpga_idx = 0
   urams_utilized = 0
   num_layers = len(network_spec['layers'])
   enough_urams = True
   for i in range(num_layers+1):
      if i == num_layers or network_spec['layers'][i]['layer_type'] == 'fpga-sep':
         # Add the output URAMs of the last layer.
         urams_utilized += utils.calc_num_urams(network_spec['layers'][i-1], "output")
         # Verify there are enough URAMs on this FPGA.
         part = fpgas[fpga_idx]
         urams_available = fpga_info[part]['URAM']
         print("FPGA {} ({}): ".format(fpga_idx, part), end='')
         print("Min URAMs utilized: {}, URAMs available: {}".format(urams_utilized, urams_available), end='')
         if urams_utilized > urams_available:
            enough_urams = False
            print("  (ERROR: Not enough URAMs on this FPGA)")
         else:
            print()
         # Reset the count for the next FPGA
         fpga_idx += 1
         urams_utilized = 0
      else:
         layer = network_spec['layers'][i]
         urams_utilized += utils.calc_num_urams(layer, "input")

   return enough_urams

def gen_network_layers(network_spec, odir, args):
   complete_layer_specs(network_spec)
   uram_reqs_satisfied = check_uram_requirements(network_spec)
   if uram_reqs_satisfied:
      print("UltraRAM requirements satisfied.")
   else:
      print("UltraRAM requirements were not satisfied. Aborting generation.")
      return
   print("Generating layers for {} network.".format(network_spec['name']))
   layers = network_spec['layers']
   # Before generating all the layers, we might be able to speed things up by
   # increasing the minimum II to the largest minimum layer latency. This enables
   # us to eliminate certain design points for certain layers so we don't have to 
   # synthesize them.
   max_min_latency = -1
   for layer in layers:
      options = layer_gen.GetLayerImplOptions(layer, args.min_ii, args.max_ii, False)
      min_l = min([l for (r, o, l, ii) in options])
      if min_l > max_min_latency:
         max_min_latency = min_l

   if max_min_latency > args.min_ii:
      print("Adjusting target minimum ii to {} cycles".format(max_min_latency))
      args.min_ii = max_min_latency

   # Put all the layers under "layers" subdirectory
   for layer in layers:
      layer_odir = os.path.join(odir, "layers/" + layer['layer_name'])
      layer_gen.generate_layer(layer, layer_odir, args)

   # Generate the AXI I/O layers.
   axi_in_spec  = {'layer_type': 'axi_in'}
   axi_out_spec = {'layer_type': 'axi_out'}
   keys = ['name', 'AXIS_WUser', 'AXIS_WId', 'AXIS_WDest'] + utils.get_top_level_keys()
   for key in keys:
      if key in network_spec:
         axi_in_spec[key] = network_spec[key]
         axi_out_spec[key] = network_spec[key]
   axi_in_spec['height']  = layers[0]['input_height']
   axi_in_spec['width']   = layers[0]['input_width']
   axi_in_spec['chans']   = layers[0]['input_chans']
   axi_out_spec['height'] = layers[-1]['output_height']
   axi_out_spec['width']  = layers[-1]['output_width']
   axi_out_spec['chans']  = layers[-1]['output_chans']
   axi_in_dir  = os.path.join(odir, "layers", "axi_in")
   axi_out_dir = os.path.join(odir, "layers", "axi_out")
   layer_gen.generate_layer(axi_in_spec, axi_in_dir, {})
   layer_gen.generate_layer(axi_out_spec, axi_out_dir, {})
   
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
      lt = l['layer_type']
      ih = l['input_height']
      iw = l['input_width']
      ic = l['input_chans']
      icp = 4 if ic == 3 else ic
      oh = l['output_height']
      ow = l['output_width']
      oc = l['output_chans']
      fs = l['filter_size']
      if 'intermediate_chans' in l:
         mc = l['intermediate_chans']
      last = (i == len(layers) - 1)
      ifmaps  = '{}_fmaps'.format(name)
      ofmaps  = 'final_fmaps' if last else '{}_fmaps'.format(layers[i+1]['layer_name'])
      filters = '{}_filters'.format(name)
      l2_filters = '{}_l2_filters'.format(name)
      in_dims   = '[{}][{}][{}]'.format(ih, iw, icp)
      out_dims  = '[{}][{}][{}]'.format(oh, ow, oc)
      filt_dim1 = mc if lt == 'conv-conv' else oc
      filt_dims = '[{}][{}][{}][{}]'.format(filt_dim1, fs, fs, ic)
      fmap_decls   += s + 'data_t {}{};\n'.format(ifmaps, in_dims)
      filter_decls += s + 'data_t {}{};\n'.format(filters, filt_dims)
      if lt == 'conv-conv':
         l2_filt_dims = '[{}][{}]'.format(oc, mc)
         filter_decls += s + 'data_t {}{};\n'.format(l2_filters, l2_filt_dims)
         layer_calls  += s + '{}({}, {}, {}, {});\n'.format(name, ifmaps, ofmaps, filters, l2_filters)
         layer_decls += 'void {}(\n  data_t in_data{},\n  data_t out_data{},\n  data_t l1_filter_data{},\n data_t l2_filter_data{});\n\n'\
            .format(name, in_dims, out_dims, filt_dims, l2_filt_dims)
      else:
         layer_calls  += s + '{}({}, {}, {});\n'.format(name, ifmaps, ofmaps, filters)
         layer_decls += 'void {}(\n  data_t in_data{},\n  data_t out_data{},\n  data_t filter_data{});\n\n'\
            .format(name, in_dims, out_dims, filt_dims)
      # TCL declaration of path to layer implementation
      layer_dicts += s + '[dict create name {} path {} type {}] \\\n'.format(name, layer_impls[name], lt)

   # Final feature maps
   fmap_decls += s + 'data_t final_fmaps[{}][{}][{}];\n'.format( \
      layers[-1]['output_height'], layers[-1]['output_width'], layers[-1]['output_chans'])
  
   substitutions = copy.copy(network_spec)
   substitutions.pop('layers', None)
   substitutions.update({
      "fmap_declarations"   : fmap_decls,
      "filter_declarations" : filter_decls,
      "layer_calls"         : layer_calls,
      "layer_declarations"  : layer_decls,
      "layer_dicts"         : layer_dicts
   })
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
def gen_network_implementations(network_spec, network_root_dir, args):
   print("Generating network implementations for network {}.".format(network_spec['name']))
   complete_layer_specs(network_spec)
   network_root_abs = os.path.abspath(network_root_dir)
   impl_top_dir = os.path.join(network_root_abs, 'impls')
   # First read the network implementations JSON file
   fp = os.path.join(network_root_dir, network_spec['name'] + "_implementations.json")
   network_impls = utils.read_json(fp)
   # For each implementation, generate it.
   for network_impl in network_impls:
      n_impl_name = network_impl['network_impl_name']
      if args.impl is not None and args.impl != n_impl_name:
         continue
      print("Generating implementation {}".format(n_impl_name))
      layer_impls = {}
      for layer in network_spec['layers']:
         lname = layer['layer_name']
         layer_impl_name = network_impl[lname]['name']
         layer_impls[lname] = os.path.join(network_root_abs, 'layers', lname, layer_impl_name)
      gen_network_impl(network_spec, impl_top_dir, n_impl_name, layer_impls)

   print("Generation complete.")


# Compare function for design points to decide which are Pareto-optimal and which are not.
# It takes in two design points, a and b, that have 'cost' and 'cycles' attributes.
# It returns True if point A has lower cost and lower cycles than point B. Otherwise,
# it returns False.
def cost_perf_compare(a, b):
   return a['cost'] <= b['cost'] and a['cycles'] <= b['cycles']

# Given a list of layers, analyzes all of their synthesis results for the different 
# implementations. Prints out a summary and returns a data structure with all the information.
def analyze_layer_synth_results(layers, network_root_dir, args):
   # layer_impls is a dict of lists of dicts where each list pertains to 
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
         true_latency = report_info['true_latency']
         est_latency  = impl['estimated_latency']
         latency_error = abs(est_latency - true_latency) / true_latency 
         layer_options.append({'name'  : impl['name'], \
                               'cost'  : report_info['cost_info']['total'], \
                               'cycles': true_latency,
                               'latency_error': latency_error})
      # Separate the layer design points into Pareto and non-Pareto optimal points.
      pareto_points, non_pareto_points = utils.pareto_sort(layer_options, cost_perf_compare)
      # Sort the layer options by execution time, highest to lowest.
      pareto_points.sort(key=lambda p: p['cycles'])
      pareto_points.reverse()
      non_pareto_points.sort(key=lambda p: p['cycles'])
      non_pareto_points.reverse()
      print("Pareto-optimal ({}):".format(len(pareto_points)))
      for p in pareto_points:
         le = p['latency_error']
         s = "  (HIGH LATENCY ERROR: {:.2%})".format(le) if le > 0.05 else ""
         print("{}: cost: {:.4f}, cycles: {:,}".format(p['name'], p['cost'], p['cycles']) + s)
      print("\nNot Pareto-optimal ({}):".format(len(non_pareto_points)))
      for p in non_pareto_points:
         le = p['latency_error']
         s = "  (HIGH LATENCY ERROR: {:.2%})".format(le) if le > 0.05 else ""
         print("{}: cost: {:.4f}, cycles: {:,}".format(p['name'], p['cost'], p['cycles']) + s)
      print('\n')
      # When choosing combinations of layer implementations to form entire networks,
      # we only want to consider the points that are Pareto-optimal.
      layer_impls[lname] = pareto_points

   return layer_impls


# Given all of the implementation results of the network layers, generates every possible
# network design point that "makes sense" given their latency. For each design point, changing
# any layer implementation to the next-slowest version would make the overall network 
# implementation slower.
def get_network_design_points(layer_impls):
   # Each list in layer_impls is a list of options for one layer sorted from slowest to fastest.
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
      cost = 0
      for l in layer_impls.keys():
         layer_choice = layer_impls[l][0]
         network_impl[l] = layer_choice
         cycles = layer_choice['cycles']
         cost += layer_choice['cost']
         max_latency = max(max_latency, cycles)
      network_impl['ii'] = max_latency + 1
      network_impl['cost'] = cost
      for l in layer_impls.keys():
         if layer_impls[l][0]['cycles'] == max_latency:
            layer_impls[l].pop(0)
            if len(layer_impls[l]) == 0:
               stop = True

      network_implementations.append(network_impl)

   # Filter out any implementations that are not Pareto-optimal
   def network_design_point_compare(a, b):
      return a['cost'] <= b['cost'] and a['ii'] <= b['ii']
   optimal_points, non_optimal_points = utils.pareto_sort(network_implementations, network_design_point_compare)
   return optimal_points


# Given a large set of design points for an overall network, 
def select_network_options(network_options, num_options):
   # TODO: Add comments explaining this code
   from sklearn.cluster import KMeans
   import numpy as np
   print("Grouping {} network implementation options into {} clusters.".format( \
      len(network_options), num_options))
   iis = []
   ii_lookup = {}
   for option in network_options:
      iis.append(option['ii'])
      ii_lookup[option['ii']] = option
   iis_log_np = np.log(np.array(iis)).reshape(-1, 1)
   cluster_indices = KMeans(n_clusters=num_options).fit_predict(iis_log_np)
   clusters = [[] for _ in range(num_options)]
   for index, ii in enumerate(iis):
      clusters[cluster_indices[index]].append(ii_lookup[ii])
   selected_options = []
   for i, c in enumerate(clusters):
      print("Cluster {} IIs: ".format(i), end='')
      for p in c:
         print('{:,} , '.format(p['ii']), end='')
      print()
      selected_options.append(min(c, key=lambda p: p['cost']))
   selected_options.sort(key=lambda p: p['cost'])
   return selected_options


# Analyzes the synthesized layer results and outputs a list of intelligently-chosen
# design points of different layer implementations to get overall network implementations
# that have a cost/performance trade-off.
def analyze_network_options(network_spec, network_root_dir, args):
   print("Analyzing layer synthesis results to create network implementation options.\n")
   complete_layer_specs(network_spec)
   layers = network_spec['layers']
   layer_impls = analyze_layer_synth_results(layers, network_root_dir, args)
   network_impls = get_network_design_points(layer_impls)
   print("Generated {} total possible network implementations.".format(len(network_impls)))
   if args.network_options is not None and args.network_options < len(network_impls):
      selected_impls = select_network_options(network_impls, args.network_options)
   else:
      selected_impls = network_impls
   print("\nSelected {} implementations as candidates for generation.".format(len(selected_impls)))
   # Print out a report of each possible implementation, and print out a JSON file with a summary of
   # the implementations.
   for i, network_impl in enumerate(selected_impls):
      iname = "i" + str(i+1)
      network_impl['network_impl_name'] = iname
      print("\nImplementation {}:".format(iname))
      for layer in layers:
         lname = layer['layer_name']
         layer_impl = network_impl[lname]
         print("{}: {}, latency = {:,} cycles, cost = {:.4f}".format( \
            lname, layer_impl['name'], layer_impl['cycles'], layer_impl['cost']))
      print("Network pipeline II: {:,} cycles".format(network_impl['ii']))
      print("Total cost: {:.4f}".format(network_impl['cost']))
   print('\n')
   # Dump selected_impls to a file
   output_json_fp = os.path.join(network_root_dir, network_spec['name'] + "_implementations.json")
   utils.write_json(output_json_fp, selected_impls)
   print("Wrote network implementation candidates to {}\n".format(output_json_fp))
