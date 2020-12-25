# hls_reports.py
# Code to parse the reports generated by Vivado HLS
import re
import xml.etree.ElementTree as ET

def read_report_xml(xml_report_filepath):
   tree = ET.parse(xml_report_filepath)
   return tree.getroot()

def GetWorstCaseLatency(report_xml):
   return int(report_xml.find('PerformanceEstimates').find('SummaryOfOverallLatency').find('Worst-caseLatency').text)

def GetCostInfo(report_xml):
   resources = report_xml.find('AreaEstimates').find('Resources')
   available_resources = report_xml.find('AreaEstimates').find('AvailableResources')
   cost = {}
   cost_factors = [('bram', 'BRAM_18K'), ('dsp', 'DSP48E'), ('ff', 'FF'), ('lut', 'LUT')]
   # "Total cost" is the sum of the percentages of each resource in the FPGA that is utilized.
   # This is a common cost function used in similar works.
   # TODO: Make cost function configurable, might just want to consider BRAMs or DSPs.
   total_cost = 0.0
   for name, rpt_name in cost_factors:
      resource_cost = int(resources.find(rpt_name).text) / int(available_resources.find(rpt_name).text)
      cost[name] = resource_cost
      total_cost = total_cost + resource_cost
   cost['total'] = total_cost
   return cost

# This method is used to parse the human-readable report generated
# for the dataflow pipeline inside the top loop.
# Ideally we wouldn't need to do this and could just parse the XML files,
# but for some reason, the XML files do not list the functions within the 
# dataflow pipeline. Since these functions can potentially vary between
# different implementations of the same layer, it would not be a good idea
# to hardcode them.
#
# This method returns a list of dicts that indicates the latencies of each 
# stage in the pipeline. Stages with a latency of 0 are not reported.
def GetDataflowStageLatencies(dataflow_rpt_filepath):
   stages = []
   with open(dataflow_rpt_filepath, 'r') as rpt:
      # Find the line with the word "Module" in it
      for line in rpt:
         if "Module" in line:
            break
      # Skip the next line
      rpt.readline()
      # Now parse lines until we find one that matches "---"
      # For each one, split it on "|" surrounded by whitespace characters (greedy)
      # Index [2] is the module name, index [8] is the worst-case latency.
      for line in rpt:
         if "---" in line:
            break
         tokens = re.split(r'\s*\|\s*', line)
         stage  = tokens[2]
         cycles = int(tokens[8])
         if cycles > 0:
            stages.append({'name': stage, 'latency': cycles})
   return stages


# Test GetDataflowStageLatencies function
if __name__ == "__main__":
   import sys
   res = GetDataflowStageLatencies(sys.argv[1])
   print(res)
