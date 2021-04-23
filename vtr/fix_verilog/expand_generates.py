import sys
import re

generate_regex = re.compile(r'^generate\s*\n(.*?)\n\s*endgenerate', (re.DOTALL | re.MULTILINE))

def process_file(ifname, ofname):
   filetext = ''
   with open(ifname, 'r') as f:
      filetext = f.read()
   filetext_fixed = filetext
   num_matches = 0
   # For each generate construct in the code
   for m in re.finditer(generate_regex, filetext):
      num_matches = num_matches + 1
      full_generate_text = m.group(0)
      inside_generate = m.group(1)
      # Remove the first and last lines (the for loop)
      loop_body = '\n'.join(inside_generate.split('\n')[1:-1])
      # Substitue the loop body with "0" for i and "1" for i
      sub_0 = re.sub(r'(\W)i(\W)', r'\1 0 \2', loop_body)
      sub_1 = re.sub(r'(\W)i(\W)', r'\1 1 \2', loop_body)
      # Append _0 or _1 to the instance names.
      sub_0 = re.sub(r'(\w+_U)', r'\1_0', sub_0)
      sub_1 = re.sub(r'(\w+_U)', r'\1_1', sub_1)
      replacement = sub_0 + '\n' + sub_1
      # Replace the generate construct with the expanded version.
      filetext_fixed = filetext_fixed.replace(full_generate_text, replacement)
      
   # Comment out all genvars
   filetext_fixed = filetext_fixed.replace('genvar', '//genvar')
   print("Expanded {} for-generates.".format(num_matches))

   # Write the new file contents to the file.
   with open(ofname, 'w') as f:
      f.write(filetext_fixed)

if __name__ == '__main__':
   process_file(sys.argv[1], sys.argv[2])
