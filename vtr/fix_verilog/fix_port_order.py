import sys
import utils
import re

module_decl_pattern = re.compile(r'module\s+(\w+)')


# Given a bunch of lines, finds the index of the first line that matches the pattern,
# beginning its search at a certain line offset.
def find_first(all_lines, pattern, offset, backwards=False, MAX_LINES=1000):
   if backwards:
      lower_index = max(0, offset-MAX_LINES)
      upper_index = offset
   else:
      lower_index = offset
      upper_index = min(len(all_lines)-1, offset+MAX_LINES)
   lines = all_lines[lower_index:upper_index+1]
   if backwards:
      it = reversed(range(len(lines)))
   else:
      it = range(len(lines))
   for i in it:
      line = lines[i]
      if re.search(pattern, line) is not None:
         return lower_index + i
   raise Exception('Did not find pattern within {} lines'.format(MAX_LINES))


# Given a module name and the line its declaration begins,
# determines the ports of the module.
# It makes some assumptions that make its job easier.
#
# Starting at the line following the declaration, look for the first line that matches ');'
# Then, between the declaration and that line, look for anything that matches this:
port_decl_pattern = re.compile(r'([a-z]\w*\s*)(,|\);|$)')
# Match 1 should then be a port name
def get_module_ports(lines, module_name, decl_start):
   ports = []
   try:
      decl_end = find_first(lines, r'\);', decl_start)
   except:
      print(("Could not find end of declaration for module {} starting on line {}"
             " by line {}").format(module_name, decl_start, decl_start + 1000))
      sys.exit(-1)
   decl_lines = lines[decl_start+1:decl_end+1]
   #print("{}: Searching for ports from line {} to {}".format(module_name, decl_start, decl_end))
   for line in decl_lines:
      # Skip comments and parameter lines
      if '//' in line or 'parameter' in line:
         continue
      match = re.search(port_decl_pattern, line)
      if match is not None:
         ports.append(match.group(1))
   #print("Found {} ports.".format(len(ports)))
   return ports


# Given a module name and the line its instantiation begins,
# returns the lines that comprise the port association
# We try to match this regex to find the port connections:
port_connection_pattern = re.compile(r'\s*\.([a-z]\w+)\(\w+\)')
# ASSUMPTION that ports can be distinguished from parameters
# by assuming that the first character in the portname is always a lower-case letter.
# We first find ');' and then find the closest '(' that comes before it (that didn't also have word characters before it)
def get_module_inst_port_connections(lines, module_name, inst_start):
   ports_end   = find_first(lines, r'\);', inst_start, False)
   ports_start = find_first(lines, r'^[^\.]*\(', ports_end, True)
   #print("{}: Found instance port connections on lines {}-{}".format(module_name, ports_start, ports_end))
   return lines[ports_start+1:ports_end], ports_start+1, ports_end

# Given a module name and list of lines in the file,
# returns a list of every line number where an instantition of that module begins.
def find_instantiations(module_name, lines):
   inst_pattern = re.compile(r'\s*' + module_name + r'(\s|$)') # non-word character or end of line after module name,
                                                             # or else we get false positives when module name is a subset of another. 
   inst_lines = []
   for i, line in enumerate(lines):
      # VERY important - match instead of search!
      # Because we want to ignore the module declaration and where it is included in comments.
      if re.match(inst_pattern, line) is not None:
         inst_lines.append(i)
   return inst_lines


def reorder_connections(ports, connections, module_name):
   ordered = []
   unmatched_ports = []
   # Go through the ports in order
   for port in ports:
      port = port.strip()
      # Find the connection that corresponds to it.
      pattern = re.compile(r'\.' + port + r'\s*\(')
      unmatched = True
      for conn in connections:
         if re.search(pattern, conn) is not None:
            conn_copy = conn.rstrip()
            if not conn_copy.endswith(','):
               conn_copy += ',' # Make sure all end with comma
            conn_copy += '\n'
            # Add to ordered list, remove from unordered list
            ordered.append(conn_copy)
            connections.remove(conn)
            unmatched = False
            break

      # If we get here, we were not able to find a match for the port.
      if unmatched:
         unmatched_ports.append(port)

   if len(ordered) == 0:
      raise Exception('Ordering connections failed, len(ordered) == 0')

   # Remove the comma from the last one (but not the newline)
   ordered[-1] = ordered[-1][:-2] + '\n'

   # Sometimes there may be empty lines in the port associations, just put those
   # at the bottom so the number of lines stays the same.
   for line in connections:
      if '.' not in line:
         ordered.append(line)
         connections.remove(line)

   # Report any unmatched ports / connections
   if len(unmatched_ports) > 0:
      print("WARNING: Unmatched ports for {}".format(module_name))
      print('\n'.join(unmatched_ports))
      print()

   if len(connections) > 0:
      print("WARNING: Unmatched connections for {}".format(module_name))
      print(''.join(connections))
      print()

   return ordered
      

def fix_port_order_top(ifname, ofname):
   print("Fixing port order in {}".format(ifname))
   print("This may take a few minutes...")
   lines = []
   with open(ifname, 'r') as f:
      lines = f.readlines()

   # Create a list of modules, the line their declarations start, and the lines where
   # they are instantiated.
   print("Creating list of modules and finding instantiations")
   modules = []
   for i, line in enumerate(lines):
      match = re.search(module_decl_pattern, line)
      if match is not None:
         module_name = match.group(1)
         inst_lines = find_instantiations(module_name, lines)
         modules.append((module_name, i, inst_lines))
         # For debugging
         #if len(modules) > 5:
         #   break

   print("Finding ports / connections and fixing order")
   for mname, decl_start, instances in modules:
      ports = get_module_ports(lines, mname, decl_start)
      num_decl_ports = len(ports)
      # Ignore modules where all the ports are on the same line as the declaration.
      if num_decl_ports == 0:
         continue
      for instance_line in instances:
         connection_lines, start, end = get_module_inst_port_connections(lines, mname, instance_line)
         num_connection_lines = len(connection_lines)
         for line in connection_lines:
            if '.' not in line:
               num_connection_lines = num_connection_lines - 1 # need to skip some blank lines
         if num_connection_lines != num_decl_ports:
            print("Mismatch between # declaration ports and # instantiation ports: {}".format(mname))
            print("Instantiation on line {} has {} ports.".format(instance_line, num_connection_lines))
            print("Declaration on line {} has {} ports.".format(decl_start, num_decl_ports))
            print()
            continue

         # Reorder the instantiation ports
         reordered_connections = reorder_connections(ports, connection_lines, mname)
         # Finally, replace the text.
         lines[start:end] = reordered_connections

   print("Done.")

   # Write the file to output.
   with open(ofname, 'w') as of:
      of.writelines(lines)

if __name__ == "__main__":
   fix_port_order_top(sys.argv[1], sys.argv[2])
   
 
