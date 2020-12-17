import math


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
      tree_stages.append(adders, num_els)
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
   ol = tree_stages[-1][1] # number of outputs of the last tree stage.
   latency = (ADD_LATENCY*tree_height) + trip_count + OVERHEAD
   return {'wrpc': wrpc, 'th': th, 'OL': ol, 'est_lat': latency, \
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
         ol = tree_stages[-1][1] # number of outputs of the last tree stage.
         new_point = {'wrpc': wrpc, 'th': th, 'OL': ol, 'est_lat': latency, \ 
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

# Generates the parameters for a simple accumulation stage.
def SimpleStage(curr_IL):
   accum_stage = {}
   accum_stage['type'] = 'simple'
   accum_stage['OL']   = 1
   # Estimated latency = add_latency * ceil(log2(IL))
   estimated_latency = ADD_LATENCY * math.ceil(math.log2(curr_IL)) + OVERHEAD
   accum_stage['est_lat'] = estimated_latency
   return accum_stage

# Given a target latency, input length, and input read bandwidth, determines
# the ideal parameters for accumulation stages
def GetAccumulationStageParams(target_latency, input_length, input_read_bw):
   accum_stages = []
   curr_IL = input_length
   first_stage = True
   while (curr_IL > 1):
      if curr_IL <= 8:
         # If there are 8 or fewer inputs, we can just do a simple
         # hard-coded accumulation down to one element. This is likely to be
         # the final accumulation stage for many layers.
         accum_stage = SimpleStage(curr_IL)
      elif first_stage and ((curr_IL / input_read_bw) > 15):
         # If the input length is sufficiently large enough, we can use an interleaved
         # accumulation stage for the first stage. Since this stage will perform a 
         # relatively drastic reduction, it is unlikely that we would want one beyond 
         # the first stage.
         accum_stage = InterleavedStage(target_latency, curr_IL, input_read_bw):
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
            accum_stage = PipelinedTreeStageComplete(target_latency, curr_IL):

      first_stage = False
      accum_stage['IL'] = curr_IL
      accum_stages.append(accum_stage)
      curr_IL = accum_stage['OL']

   return accum_stages
