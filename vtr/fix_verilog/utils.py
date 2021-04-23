import re

# Find a particular line that matches a pattern
def find_line(lines, pattern, index=0):
   matches = []
   for i, line in enumerate(lines):
      if re.search(pattern, line) is not None:
         matches.append((i, line))
   if len(matches) == 0:
      raise Exception('Did not find pattern {}'.format(pattern))
   if len(matches) < index+1:
      raise Exception('Did not find {} matches for pattern {}'.format(index+1, pattern))
   return matches[index]
