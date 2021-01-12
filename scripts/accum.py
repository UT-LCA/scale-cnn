# accum.py
# Code for choosing accumulation stages for a layer,
# and generating th C code for those stages.
import math
import os
import string
import copy


# Design point compare function for pipelined tree accumulation stages
# where the inputs are complete-partitioned.
# Returns True if point1 is better than point2
# First we compare output length, then words read per cycle, then tree height
# For all three, smaller is better.
def compare_points(point1, point2):
   if point1['OL'] == point2['OL']:
      if point1['wrpc'] == point2['wrpc']:
         return point1['th'] < point2['th']
      else:
         return point1['wrpc'] < point2['wrpc']
   else:
      return point1['OL'] < point2['OL']
   
ADD_LATENCY = 3  # Latency of a half-precision floating point addition
OVERHEAD    = 1  # Always 1 cycle of overhead for each stage

# Given the words read per cycle and tree height,
# returns a description of each substage of the pipelined tree accumulation stage.
# It is a list of tuples of format (# adders, # outputs of this substage)
def GetPipelinedTreeStageSubstages(wrpc, th):
   if th == 0:
      raise Exception('Pipelined tree stage with tree height of 0')
   tree_stages = []
   num_els = wrpc
   for i in range(th):
      adders  = math.floor(num_els / 2)
      num_els = adders + (num_els % 2)
      tree_stages.append((adders, num_els))
   return tree_stages


# Function for choosing a pipelined tree accumulation stage with
# non-complete-partitioned inputs (inputs stored in partitioned BRAMs).
def PipelinedTreeStageNoncomplete(target_latency, curr_IL, input_read_bw):
   # For these stages, the maximum tree height is limited by one of two things:
   # the number of words we can read per cycle, or the target latency
   # Read bandwidth can limit the tree height because we always want an II of 1.
   # We must be able to supply all first-level adders with new values each cycle.
   total_overhead = OVERHEAD
   max_tree_height_read_bw_limited = math.ceil(math.log2(input_read_bw))
   trip_count = math.ceil(curr_IL / input_read_bw)
   max_tree_height_latency_limited = math.floor((target_latency - trip_count - total_overhead) / ADD_LATENCY)
   # In some extreme cases, the latency-limited tree height is 0. In this case, we have no choice but 
   # to just accept that accumulation will be the bottleneck stage. So just choose a tree height of 1
   # in this case
   tree_height = min(max_tree_height_read_bw_limited, max_tree_height_latency_limited)
   tree_height = max(tree_height, 1)
   tree_stages = GetPipelinedTreeStageSubstages(input_read_bw, tree_height)
   latency = (ADD_LATENCY*tree_height) + trip_count + total_overhead
   num_last_level_outputs = tree_stages[-1][1]
   ol = num_last_level_outputs * trip_count
   # Determine whether the outputs will be stored in BRAMs or registers.
   # For pipelined tree accumulation stages that have more than 6 outputs, we will store the 
   # outputs in BRAMs. All other accumulation stages will store outputs in registers.
   # The threshold of 6 was chosen heuristically. The reason we do this is that complete-
   # partitioned registers with a large number of elements infers a great deal of logic to
   # determine when to write to each register. Putting them in a single BRAM is a much better
   # design choice because LUTs are generally more limited than BRAMs in these designs.
   # Also, pick registers over BRAMs if we would only write to each BRAM once (trip count of 1)
   output_storage = 'bram' if (ol > 6 and trip_count > 1) else 'regs'
   # We will partition the outputs with a factor equal to the amount of outputs we write per cycle.
   # It might be possible to do just half, need to experiment (TODO)
   stage = {'wrpc': input_read_bw, 'th': tree_height, 'OL': ol, 'est_lat': latency,
           'output_storage_type': output_storage, 'substages': tree_stages, 'type': 'pipelined_tree'}
   if output_storage == 'bram':
      stage['bram_part_factor'] = num_last_level_outputs
      stage['next_read_bw'] = num_last_level_outputs * 2
   else:
      stage['next_read_bw'] = ol
   return stage


# Function for choosing a pipelined tree accumulation stage with
# complete-partitioned inputs.
def PipelinedTreeStageComplete(target_latency, curr_IL):
   # When the inputs are complete-partitioned, we have no limitation on words read per cycle.
   # This means we can explore different values of tree height and words read per cycle.
   # This is an optimization problem. The search space is as follows:
   #
   # Minimum words read per cycle = 2
   # Maximum words read per cycle = input length
   # Minimum tree height = 1
   # Maximum tree height = ceil(log2(words read per cycle))
   #
   # The constraints are:
   #  - Words read per cycle must be even.
   #  - The calculated latency for the design point cannot exceed the target latency
   #
   # For the "cost" function see compare_points()
   #
   # The design space is not that large so we can just do a brute-force search.
   points = []
   for wrpc in range(2, curr_IL+2, 2):
      trip_count = math.ceil(curr_IL / wrpc)
      for th in range(1, math.ceil(math.log2(wrpc))+1):
         latency = (ADD_LATENCY*th) + trip_count + OVERHEAD
         if latency > target_latency:
            continue
         # Calculate the output length and # adders per stage of the tree.
         # Each stage does a 2x reduction, but have to handle odd numbers correctly.
         tree_stages = GetPipelinedTreeStageSubstages(wrpc, th)
         # Output length = # outputs of the last stage * # of times inputs are read
         #               = # outputs of last stage * trip count
         ol = tree_stages[-1][1] * trip_count
         # Each function call, each DSP produces ceil(IL / wrpc) outputs.
         dsp_util = trip_count
         new_point = {'wrpc': wrpc, 'th': th, 'OL': ol, 'est_lat': latency, 
                      'substages': tree_stages, 'dsp_util': dsp_util}
         points.append(new_point)


   # There can be multiple points that satisfy the latency requirement. The different points
   # will have a trade-off between DSP usage and output length.
   #
   # A naive approach would be to always pick the design point with the smallest output length.
   # This would minimize the total amount of accumulation stages required for the overall layer.
   # But this isn't a really great design choice because it could use a lot of DSPs that are only
   # used once or twice per function call. Using fewer DSPs but having one or two additional accum 
   # stages is better. It will not affect the initiation interval of the dataflow pipeline,
   # and will barely increase the overall latency of the layer as a whole.
   #
   # What we really should consider is the DSP utilization, i.e., how many times each adder is 
   # used per function call. The higher the utilization, the fewer DSPs used overall. However, we 
   # wouldn't want to pick the one with the absolute fewest DSPs possible either, as then we 
   # would just have a bunch of stages where each one uses a single DSP.
   #
   # So to strike a balance, I will use this heuristic: Choose the design point with the smallest
   # output length, that uses each DSP at least 4 times per function call. If no such points exist,
   # try with 3 times, then 2, then 1.
   best_point = None
   ideal_min_dsp_util = 4  # DSP utilization in units of "uses per function call"
   for min_dsp_util in reversed(range(1, ideal_min_dsp_util+1)):
      for point in points:
         if point['dsp_util'] < min_dsp_util:
            continue
         if best_point is None or compare_points(point, best_point):
            best_point = point
      if best_point is not None:
         break # Found at least one point with this min_dsp_util.

   # Something went wrong if we didn't find any valid points.
   if best_point is None:
      raise Exception('Something went wrong with the accum stage generation algorithm.')

   best_point['type'] = 'pipelined_tree'
   return best_point

# Generates the parameters for an interleaved accumulation stage.
def InterleavedStage(target_latency, curr_IL, input_read_bw):
   accum_stage = {}
   accum_stage['type'] = 'interleaved'
   # The stage will reduce the inputs to 4*input_read_bw outputs.
   interleave_factor = 4
   ol = interleave_factor*input_read_bw 
   accum_stage['OL'] = ol
   # The accumulators form a pipeline, but the pipeline II is equal to
   # the interleave factor, not 1.
   # Due to the interleaving, it's really an effective II of 1, but the 
   # synthesizer doesn't know that.
   trip_count = math.ceil(curr_IL / ol)
   # This stage stores its outputs in a single BRAM
   # It always has at least 8 outputs, and since it has twice the read bandwidth
   # of the dot_product stage, the additional time it takes to write everything to 
   # the BRAM should not exceed the target latency.
   accum_stage['output_storage_type'] = 'bram'
   accum_stage['bram_part_factor']    = 1
   accum_stage['next_read_bw']        = 2
   # Add 3 cycles to wait for the final addition to finish (latency of the pipeline)
   # Then another OL/2 + 1 cycles for writing the outputs.
   estimated_latency = interleave_factor*trip_count + 3 + int(ol/2) + 1
   accum_stage['est_lat'] = estimated_latency
   return accum_stage

# Generates the parameters for an unpipelined tree accumulation stage.
def UnpipelinedTreeStage(curr_IL):
   accum_stage = {}
   accum_stage['type'] = 'unpipelined_tree'
   accum_stage['OL']   = 1
   # Estimated latency = add_latency * ceil(log2(IL))
   # Interestingly, experiments showed that the actual latency of this stage
   # is one less than expected (two less than expected if including one cycle
   # overhead). I'm not sure why this is, but going with it because these 
   # estimates should be as accurate as possible.
   estimated_latency = ADD_LATENCY * math.ceil(math.log2(curr_IL)) - 1
   accum_stage['est_lat'] = estimated_latency
   return accum_stage

# Generates the parameters for a simple loop accumulation stage.
def SimpleLoopStage(curr_IL):
   accum_stage = {}
   accum_stage['type'] = 'simple_loop'
   accum_stage['OL']   = 1
   # Estimated latency = add_latency * IL
   estimated_latency = ADD_LATENCY * curr_IL + OVERHEAD
   accum_stage['est_lat'] = estimated_latency
   return accum_stage

# Given a target latency, input length, and input read bandwidth, determines
# the ideal parameters for accumulation stages
def GetAccumulationStageParams(target_latency, input_length, input_read_bw):
   accum_stages = []
   curr_IL = input_length
   curr_input_read_bw = input_read_bw
   curr_input_storage_type = 'bram'
   while (curr_IL > 1):
      if (ADD_LATENCY * curr_IL) + OVERHEAD < target_latency:
         # If the target latency is sufficiently large, we may have enough time to 
         # place a simple loop accumulation stage. This is the slowest but cheapest 
         # possible accumulation stage. It uses a single adder and has no pipelining.
         # This stage always reduces the inputs to 1 output, so it will always be the 
         # last stage.
         accum_stage = SimpleLoopStage(curr_IL)
      elif curr_input_read_bw >= curr_IL and curr_IL <= 8 and (ADD_LATENCY * math.ceil(math.log2(curr_IL)) - 1 < target_latency):
         # The next option is an unpipelined tree. This will also reduce it down to 1 element.
         # This is a good option when the number of inputs is small but we don't have time for
         # a simple loop stage. We should only use this if the number of inputs is small.
         # Otherwise the number of DSPs  will be approximately equal to the input length, and 
         # each will only be used once per function call. This is a wasteful use of FPGA resources.
         # In these scenarios it is better to use a pipelined tree stage that would use each DSP
         # multiple times. I heuristically chose 8 as the size limit for this stage type.
         #
         # This stage also fundamentally requires that all inputs can be read in a single cycle.
         accum_stage = UnpipelinedTreeStage(curr_IL)
      elif curr_input_storage_type == 'bram' and ((curr_IL / curr_input_read_bw) > 15):
         # If the input length is sufficiently large enough and is stored in BRAMs, we 
         # can use an interleaved accumulation stage. Since this stage will perform a 
         # relatively drastic reduction, it is unlikely that we would want one beyond 
         # the first stage.
         #
         # Why 15? In this stage there are 4*input_read_bw accumulators. I have somewhat
         # arbitrarily chosen a heuristic that we should only use this stage if each 
         # accumulator is used 4 times in order to get "decent" utilization. This would 
         # imply I should compare it to 16 but I subtracted one for "close calls".
         accum_stage = InterleavedStage(target_latency, curr_IL, curr_input_read_bw)
      else:
         # Otherwise, insert a pipelined tree accumulation stage
         #
         # This stage is characterized by two parameters: tree height and words read per cycle.
         # The inputs can be stored in either BRAMs or registers.
         if curr_input_storage_type == 'bram':
            accum_stage = PipelinedTreeStageNoncomplete(target_latency, curr_IL, curr_input_read_bw)
         else:
            accum_stage = PipelinedTreeStageComplete(target_latency, curr_IL)


      # If not specified by the function that generated the stage, the outputs will be stored in registers
      if 'output_storage_type' not in accum_stage:
         accum_stage['output_storage_type'] = 'regs'
         accum_stage['next_read_bw'] = accum_stage['OL']

      accum_stage['IL'] = curr_IL
      accum_stage['stage_num'] = len(accum_stages) + 1
      accum_stages.append(accum_stage)

      # Prepare to choose the next stage
      curr_IL =                 accum_stage['OL']
      curr_input_storage_type = accum_stage['output_storage_type']
      curr_input_read_bw      = accum_stage['next_read_bw']

      # Something has gone wrong if we have more than 10 stages
      if len(accum_stages) > 10:
         raise Exception('Number of accumulation stages is too large.')

      ## end while loop

   return accum_stages



TEMPLATE_DIR = os.path.join(os.getenv('SCALE_CNN_ROOT'), 'templates/shared')
def read_template(template_fname):
   global TEMPLATE_DIR
   tpath = os.path.join(TEMPLATE_DIR, template_fname)
   with open(tpath, 'r') as f:
      return string.Template(f.read())


# Generates the code for an unpipelined tree accumulation stage
def GenUnpipelinedTreeAccumStage(stage):
   # Generate the body (all the intermediate sums)
   val_queue = ['accum_in[{}]'.format(x) for x in range(stage['IL'])]
   body = ""
   int_sum_count = 0
   while len(val_queue) > 1:
      op1  = val_queue.pop()
      op2  = val_queue.pop()
      sum_ = "sum" + str(int_sum_count)
      int_sum_count = int_sum_count + 1
      body += "   data_t {} = {} + {};\n".format(sum_, op1, op2)
      val_queue.insert(0, sum_)
   body += "   return {};\n".format(val_queue[0])
   # Read the template and make substitutions
   template = read_template('accum_unpipelined_tree.c')
   subs = copy.copy(stage)
   subs['body'] = body
   return template.substitute(subs)


# Generates the code for an interleaved accumulation stage
def GenInterleavedAccumStage(stage):
   # There is no custom body to generate for this one
   # So just read the template and make substitutions.
   template = read_template('accum_interleaved.c')
   return template.substitute(stage)

# Generates the code for a simple loop stage
def GenSimpleLoopAccumStage(stage):
   # Same deal as above
   template = read_template('accum_simple.c')
   return template.substitute(stage)

# Generates the code for a pipelined tree accumulation stage
def GenPipelinedTreeAccumStage(stage):
   # Generate the body (all the intermediate sums)
   # This part of the code is similar to the simple accumulation stage
   # But there is one additional part that is handled in the template code.
   val_queue = ['vals[{}]'.format(x) for x in range(stage['wrpc'])]
   body = ""
   int_sum_count = 0
   s = " " * 6
   # Calculate the total number of adders from the provided substage info
   total_adders = 0
   substages = stage['substages']
   for adders, num_els in substages:
      total_adders += adders
   # Add statements for the pipelined tree
   for i in range(total_adders):
      op1  = val_queue.pop()
      op2  = val_queue.pop()
      sum_ = "sum" + str(int_sum_count)
      int_sum_count = int_sum_count + 1
      body += s + "data_t {} = {} + {};\n".format(sum_, op1, op2)
      val_queue.insert(0, sum_)
   # Sanity check: At this point, the number of elements in the val queue
   # should be exactly equal to the number of outputs of the final stage.
   final_stage_outputs = substages[-1][1]
   if final_stage_outputs != len(val_queue):
      exc_str = '''Something went wrong when generating pipelined tree accum stage.
         final stage outputs ({}) does not equal length of val queue ({})'''
      exc_str = exc_str.format(final_stage_outputs, len(val_queue))
      raise Exception(exc_str)
   # Statements for assigning the final outputs to accum_out
   for o, op in enumerate(val_queue):
      body += s + "accum_out[out_idx+{}] = {};\n".format(o, op)
   body += s + "out_idx += {};\n".format(final_stage_outputs)
   # Read the template and make substitutions
   template = read_template('accum_pipelined_tree.c')
   subs = copy.copy(stage)
   subs['body'] = body
   return template.substitute(subs)


def GenerateAccumStageCode(stage):
   if stage['type'] == 'simple_loop':
      func = GenSimpleLoopAccumStage(stage)
   elif stage['type'] == 'interleaved':
      func = GenInterleavedAccumStage(stage)
   elif stage['type'] == 'pipelined_tree':
      func = GenPipelinedTreeAccumStage(stage)
   elif stage['type'] == 'unpipelined_tree':
      func = GenUnpipelinedTreeAccumStage(stage)
   else:
      raise Exception('Unknown accum stage type.')
   return func

# Generates the code that calls all of the accumulation stages in sequence.
def GenAccumStageFuncCalls(lname, ochan_sf, stage_params):
   code = ""
   s = " " * 6
   has_return_val = False
   for stage in stage_params:
      stage_num = stage['stage_num']
      op_in = 'products[%d]' if stage_num == 1 else 'accum{}_out_%d'.format(stage_num-1)
      op_out = "accum{}_out_%d".format(stage_num)
      has_return_val = stage['type'] == 'simple_loop' or stage['type'] == 'unpipelined_tree'
      if has_return_val:
         # Function call for the last stage
         for ochan in range(ochan_sf):
            code += s + "outputs[{}] = {}_accum_{}({});\n".format(ochan, lname, stage_num, (op_in % ochan))
      else:
         # Declare the outputs of the current stage
         for ochan in range(ochan_sf):
            code += s + "data_t {}[{}];\n".format(op_out % ochan, stage['OL'])
         # Array partitioning pragmas for stage outputs (if necessary)
         for ochan in range(ochan_sf):
            if stage['output_storage_type'] == 'bram':
               # Stage's outputs are stored in BRAMs
               part_factor = stage['bram_part_factor']
               if part_factor > 1:
                  code += s + "#pragma HLS array_partition variable={} cyclic factor={}\n".format(op_out % ochan, part_factor)
            else:
               # Stage's outputs are stored in registers
               code += s + "#pragma HLS array_partition variable={} complete\n".format(op_out % ochan)
         # The actual function call
         for ochan in range(ochan_sf):
            code += s + "{}_accum_{}({}, {});\n".format(lname, stage_num, op_in % ochan, op_out % ochan)
         
   if not has_return_val:
      # Function call for the last stage if the output is an array with length 1 instead of return value.
      for ochan in range(ochan_sf):
         code += s + "outputs[{}] = accum{}_out_{}[0];\n".format(ochan, stage_params[-1]['stage_num'], ochan)

   return code


# Top-level function for generating the code for the accumulation stages of a layer.
# Target latency is the maximum latency allowed for any single accumulation stage
# Input length is the number of values that need to be added together.
# Read bandwidth is the number of inputs we can read per cycle. This will either 
# be the array partitioning factor of the inputs, or twice it.
# ochan_sf is the output channel scaling factor.
def GenerateAccumulationStages(layer_name, ochan_sf, target_latency, input_length, read_bw):
   stage_params = GetAccumulationStageParams(target_latency, input_length, read_bw)
   stage_functions = []
   for stage in stage_params:
      stage['lname'] = layer_name
      stage_functions.append(GenerateAccumStageCode(stage))
   function_defs_code  = '\n\n\n'.join(stage_functions)
   function_calls_code = GenAccumStageFuncCalls(layer_name, ochan_sf, stage_params)
   return (function_defs_code, function_calls_code)

#========================================================================================
#========================================================================================

if __name__ == "__main__":
   # Four test cases:
   test_cases = [{'IL': 144, 'read_bw': 1, 'targ_lat': 146}, \
                 {'IL': 144, 'read_bw': 4, 'targ_lat':  75}, \
                 {'IL':  27, 'read_bw': 1, 'targ_lat':  30}, \
                 {'IL':  27, 'read_bw': 6, 'targ_lat':  12}]
   for i, tc in enumerate(test_cases):
      print("Test case: " + str(tc))
      lname = "l" + str(i)
      stages = GetAccumulationStageParams(tc['targ_lat'], tc['IL'], tc['read_bw'])
      for stage in stages:
         if stage['type'] == 'pipelined_tree':
            print("Type: {}, WRPC: {}, TH: {}, IL: {}, OL: {}, Estimated Latency: {}".format(stage['type'], \
               stage['wrpc'], stage['th'], stage['IL'], stage['OL'], stage['est_lat']))
         else:
            print("Type: {}, IL: {}, OL: {}, Estimated Latency: {}".format(stage['type'], \
               stage['IL'], stage['OL'], stage['est_lat']))
      print("\n")
      func_code, func_calls_code = GenerateAccumulationStages(lname, 2, tc['targ_lat'], tc['IL'], tc['read_bw'])
      print("Function definitions:\n")
      print(func_code)
      print("Function calls:\n")
      print(func_calls_code)

