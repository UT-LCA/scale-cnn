import sys
import re
import math
import os
import utils

# Get the width and depth of the shift register given the lines
def get_dims(lines):
   idx, width_line = utils.find_line(lines, 'parameter DATA_WIDTH')
   idx, depth_line = utils.find_line(lines, 'parameter DEPTH')
   w_match = re.search('d(\d+);', width_line)
   d_match = re.search('d(\d+);', depth_line)
   if w_match is None:
      raise Exception('Could not determine shift reg width.')
   if d_match is None:
      raise Exception('Could not determine shift reg depth.')
   width = int(w_match.group(1))
   depth = int(d_match.group(1))
   return width, depth

def process_shiftreg_file(ifname, ofname):
   # Read the file and get the dimensions
   lines = []
   with open(ifname, 'r') as f:
      lines = f.readlines()
   width, depth = get_dims(lines)
   
   # Fix the declaration of the shift register signals
   # Find the reg ... SRL_SIG line
   srl_sig_pattern = r'reg.*SRL_SIG'
   srl_sig_idx, srl_sig_line = utils.find_line(lines, srl_sig_pattern)
   # Declare a separate register for each element
   regs = [('sr_%d' % d) for d in range(depth)]
   # Replace it with this:
   srl_dec = 'reg[DATA_WIDTH-1:0] ' + ', '.join(regs) + ';\n'
   lines[srl_sig_idx] = srl_dec

   # Fix the assignments of the elements in the shift register.
   # Find the line that begins the for loop
   forloop_idx, forloop_line = utils.find_line(lines, r'for \(i=0')
   # Empty the next two lines
   lines[forloop_idx+1] = '\n'
   lines[forloop_idx+2] = '\n'
   # Create the assignments
   s = ' ' * 12 
   assignments = s + 'sr_0 <= data;\n'
   for d in range(depth):
      if (d == 0):
         continue
      assignments += s + ('sr_%d' % d) + ' <= ' + ('sr_%d' % (d-1)) + ';\n'
   # Put in the assignments
   lines[forloop_idx] = assignments

   # Fix the output data
   # Find the line that assigns q
   qassign_idx, qassign_line = utils.find_line(lines, 'assign q')
   # Build the always block with the case statement.
   alen = math.ceil(math.log2(depth))
   q_block  = 'always @( ' + ', '.join(regs) + ', a) begin\n'
   q_block += '   case (a)\n'
   for d in range(depth):
      q_block += (' ' * 6) + ("%d'd%d" % (alen, d)) + ': q = ' + regs[d] + ';\n'
   q_block += (' ' * 6) + ('default: q = sr_%d;\n' % (depth-1))
   q_block += '   endcase\n'
   q_block += 'end\n'
   # Put in the new assignment code
   lines[qassign_idx] = q_block

   # Write the output
   with open(ofname, 'w') as of:
      of.writelines(lines)


if __name__ == "__main__":
   # TEMP, just try with a single file right now.
   #process_shiftreg_file(sys.argv[1], sys.argv[2])
   # Get a list of files from stdin
   # Then process them one at a time, saving the output to "fixed" directory.
   files = sys.stdin.readlines()
   for fn in files:
      ifn = fn.rstrip()
      ofn = os.path.join('fixed', os.path.basename(ifn))
      process_shiftreg_file(ifn, ofn)

