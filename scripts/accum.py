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
# We want to find the design point that has the smallest output length
# but still meets the latency requirement. If multiple points have the 
# same output length, choose the one with the smallest words read per cycle.
# This will correspond to the one with the fewest number of adders.
# If those are the same, choose the one with the smallest tree length.
def compare_points(point1, point2):
   if point1['OL'] == point2['OL']:
      if point1['wrpc'] == point2['wrpc']:
         return point1['th'] < point2['th']
      else:
         return point1['wrpc'] < point2['wrpc']
   else:
      return point1['OL'] < point2['OL']
   
ADD_LATENCY = 3  # Latency of a half-precision floating point addition
OVERHEAD    = 2  # Always 1 cycle of overhead each for read and write.

# Given the words read per cycle and tree height,
# returns a description of each substage of the pipelined tree accumulation stage.
# It is a list of tuples of format (# adders, # outputs of this substage)
def GetPipelinedTreeStageSubstages(wrpc, th):
   tree_stages = []
   num_els = wrpc
   for i in range(th):
      adders  = math.floor(num_els / 2)
      num_els = adders + (num_els % 2)
      tree_stages.append((adders, num_els))
   return tree_stages


# Function for choosing a pipelined tree accumulation stage with
# non-complete-partitioned inputs.
def PipelinedTreeStageNoncomplete(target_latency, curr_IL, input_read_bw):
   # For the first stage, the maximum tree height is limited by one of two things:
   # the number of words we can read per cycle, or the target latency
   # Read bandwidth can limit the tree height because we always want an II of 1.
   # We must be able to supply all first-stage adders with new values each cycle.
   max_tree_height_read_bw_limited = math.ceil(math.log2(input_read_bw))
   trip_count = math.ceil(curr_IL / input_read_bw)
   max_tree_height_latency_limited = math.floor((target_latency - trip_count - OVERHEAD) / ADD_LATENCY)
   tree_height = min(max_tree_height_read_bw_limited, max_tree_height_latency_limited)
   tree_stages = GetPipelinedTreeStageSubstages(input_read_bw, tree_height)
   latency = (ADD_LATENCY*tree_height) + trip_count + OVERHEAD
   ol = tree_stages[-1][1] * math.ceil(curr_IL / input_read_bw)
   return {'wrpc': input_read_bw, 'th': tree_height, 'OL': ol, 'est_lat': latency,
           'substages': tree_stages, 'type': 'pipelined_tree'}


# Function for choosing a pipelined tree accumulation stage with
# complete-partitioned inputs.
def PipelinedTreeStageComplete(target_latency, curr_IL):
   # For all stages beyond the first, we have no limitation on words read per cycle.
   # This means we can explore different values of tree height and words read per cycle.
   # This is an optimization problem. The search space is as follows:
   # Minimum words read per cycle = 2
   # Maximum words read per cycle = input length
   # Minimum tree height = 1
   # Maximum tree height = ceil(log2(words read per cycle))
   # The constraints are:
   #  - Words read per cycle must be even.
   #  - The calculated latency for the design point cannot exceed the target latency
   #
   # For the "cost" function see compare_points()
   #
   # The design space is not that large so we can just do a brute-force search.
   best_point = None
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
         #               = # outputs of last stage * ceil(IL / wrpc)
         ol = tree_stages[-1][1] * math.ceil(curr_IL / wrpc)
         new_point = {'wrpc': wrpc, 'th': th, 'OL': ol, 'est_lat': latency, 
                      'substages': tree_stages, 'type': 'pipelined_tree'}
         if best_point is None or compare_points(new_point, best_point):
            best_point = new_point

   # Something went wrong if we didn't find any valid points.
   if best_point is None:
      raise Exception('Something went wrong with the accum stage generation algorithm.')

   return best_point

# Generates the parameters for an interleaved accumulation stage.
def InterleavedStage(target_latency, curr_IL, input_read_bw):
   accum_stage = {}
   accum_stage['type'] = 'interleaved'
   # The stage will reduce the inputs to 4*input_read_bw outputs.
   accum_stage['OL'] = 4*input_read_bw
   # The accumulators form an effective pipeline with II of 1
   # The trip count is ceil(curr_IL / input_read_bw)
   trip_count = math.ceil(curr_IL / input_read_bw)
   estimated_latency = ADD_LATENCY + trip_count + OVERHEAD
   accum_stage['est_lat'] = estimated_latency
   return accum_stage

# Generates the parameters for an unpipelined tree accumulation stage.
def UnpipelinedTreeStage(curr_IL):
   accum_stage = {}
   accum_stage['type'] = 'unpipelined_tree'
   accum_stage['OL']   = 1
   # Estimated latency = add_latency * ceil(log2(IL))
   estimated_latency = ADD_LATENCY * math.ceil(math.log2(curr_IL)) + OVERHEAD
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
   first_stage = True
   while (curr_IL > 1):
      if (ADD_LATENCY * curr_IL) + OVERHEAD < target_latency:
         # If the target latency is sufficiently large, we may have enough time to 
         # place a simple loop accumulation stage. This is the slowest but cheapest 
         # possible accumulation stage. It uses a single adder and has no pipelining.
         # This stage always reduces the inputs to 1 output, so it will always be the 
         # last stage.
         accum_stage = SimpleLoopStage(curr_IL)
      elif curr_IL <= 16 and (ADD_LATENCY * math.ceil(math.log2(curr_IL)) + OVERHEAD < target_latency):
         # The next option is an unpipelined tree. This will also reduce it down to 1 element.
         # This is a good option when the number of inputs is small but we don't have time for
         # a simple loop stage. We should only use this if the number of inputs is small.
         # Otherwise the number of adders will be huge and this will be too costly.
         # I will choose a limit of 16 inputs for this stage (another heuristic)
         accum_stage = UnpipelinedTreeStage(curr_IL)
      elif first_stage and ((curr_IL / input_read_bw) > 15):
         # If the input length is sufficiently large enough, we can use an interleaved
         # accumulation stage for the first stage. Since this stage will perform a 
         # relatively drastic reduction, it is unlikely that we would want one beyond 
         # the first stage.
         #
         # Why 15? In this stage there are 4*input_read_bw accumulators. I have somewhat
         # arbitrarily chosen a heuristic that we should only use this stage if each 
         # accumulator is used 4 times in order to get "decent" utilization. This would 
         # imply I should compare it to 16 but I subtracted one for "close calls".
         accum_stage = InterleavedStage(target_latency, curr_IL, input_read_bw)
      else:
         # Otherwise, insert a pipelined tree accumulation stage
         #
         # This stage is characterized by two parameters: tree height and words read per cycle.
         # If this is the first stage, the word read per cycle is fixed, since we are reading
         # from BRAMs. For all stages beyond the first, we use complete partitioning, so
         # there is no limit other than the total length of the input itself.
         if first_stage:
            accum_stage = PipelinedTreeStageNoncomplete(target_latency, curr_IL, input_read_bw)
         else:
            accum_stage = PipelinedTreeStageComplete(target_latency, curr_IL)

      first_stage = False
      accum_stage['IL'] = curr_IL
      accum_stage['stage_num'] = len(accum_stages) + 1
      accum_stages.append(accum_stage)
      curr_IL = accum_stage['OL']
      
      # Something has gone wrong if we have more than 10 stages
      if len(accum_stages) > 10:
         raise Exception('Number of accumulation stages is too large.')

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
def GenAccumStageFuncCalls(stage_params):
   code = ""
   s = " " * 6
   has_return_val = False
   for stage in stage_params:
      stage_num = stage['stage_num']
      op_in = 'products' if stage_num == 1 else 'accum{}_out'.format(stage_num-1)
      op_out = "accum{}_out".format(stage_num)
      has_return_val = stage['type'] == 'simple_loop' or stage['type'] == 'unpipelined_tree'
      if has_return_val:
         code += s + "data_t final_sum = ${{lname}}_accum_{}({});\n".format(stage_num, op_in)
      else:
         code += s + "#pragma HLS partition variable={} complete\n".format(op_out)
         code += s + "data_t {}[{}];\n".format(op_out, stage['OL'])
         code += s + "${{lname}}_accum_{}({}, {});\n".format(stage_num, op_in, op_out)
         
   if not has_return_val:
      code += s + "data_t final_sum = accum{}_out[0];\n".format(stage_params[-1]['stage_num'])

   return code

# Top-level function for generating the code for the accumulation stages of a layer.
# Target latency is the maximum latency allowed for any single accumulation stage
# Input length is the number of values that need to be added together.
# Read bandwidth is the number of inputs we can read per cycle. This will either 
# be the array partitioning factor of the inputs, or twice it.
def GenerateAccumulationStages(target_latency, input_length, read_bw):
   stage_params = GetAccumulationStageParams(target_latency, input_length, read_bw)
   stage_functions = []
   for stage in stage_params:
      stage_functions.append(GenerateAccumStageCode(stage))
   function_defs_code  = '\n\n\n'.join(stage_functions)
   function_calls_code = GenAccumStageFuncCalls(stage_params)
   return (function_defs_code, function_calls_code)

#========================================================================================
#========================================================================================

if __name__ == "__main__":
   # Four test cases:
   test_cases = [{'IL': 144, 'read_bw': 1, 'targ_lat': 146}, \
                 {'IL': 144, 'read_bw': 4, 'targ_lat':  75}, \
                 {'IL':  27, 'read_bw': 1, 'targ_lat':  30}, \
                 {'IL':  27, 'read_bw': 6, 'targ_lat':  13}]
   for tc in test_cases:
      print("Test case: " + str(tc))
      stages = GetAccumulationStageParams(tc['targ_lat'], tc['IL'], tc['read_bw'])
      for stage in stages:
         if stage['type'] == 'pipelined_tree':
            print("Type: {}, WRPC: {}, TH: {}, IL: {}, OL: {}, Estimated Latency: {}".format(stage['type'], \
               stage['wrpc'], stage['th'], stage['IL'], stage['OL'], stage['est_lat']))
         else:
            print("Type: {}, IL: {}, OL: {}, Estimated Latency: {}".format(stage['type'], \
               stage['IL'], stage['OL'], stage['est_lat']))
      print("\n")
      func_code, func_calls_code = GenerateAccumulationStages(tc['targ_lat'], tc['IL'], tc['read_bw'])
      print("Function definitions:\n")
      print(func_code)
      print("Function calls:\n")
      print(func_calls_code)

