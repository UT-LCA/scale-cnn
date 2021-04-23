# Delete old fixed directory if it exists
rm -rf fixed/
# Copy verilog over to fixed
cp -R verilog/ fixed/
# Fix shift registers
grep -il "SRL_SIG" verilog/*.v | python3 fix_shiftregs.py
# Concatenate all of the verilog files into a single file.
cat fixed/*.v > fixed/all.v
# Replace the "(* ... *)" constructs with " "
sed -i -E 's/\(\*(.*)\*\)/ /g' fixed/all.v
# Replace arrays with vectors
# E.g. replace "wire sig[3:0];" with "wire [3:0] sig;"
sed -i -E 's/^(\s*)(wire|reg)\s+(\w+)\s*(\[.*?\])\s*;/\1\2 \4 \3;/g' fixed/all.v
# Expand for-generate constructs
python3 expand_generates.py fixed/all.v fixed/all.v
# Remove "signed" and "$signed" (and unsigned) keywords - not supported by ODIN
sed -i -E 's/\$signed//g' fixed/all.v
sed -i -E 's/\$unsigned//g' fixed/all.v
sed -i -E 's/(\W)signed/\1/g' fixed/all.v
# Reverse any ranges that go from 0 to X to instead go down from X to 0
sed -i -E 's/\[0:(.*?)\]/\[\1:0\]/g' fixed/all.v
# Replace variable indexing of buf_q* signals with ternary operators (only ever indexed with 0 or 1)
sed -i -E "s/(buf_q[01])\[([a-z_]*)\]/\(\2 == 1\'b1 \? \1\[1\] : \1\[0\]\)/g" fixed/all.v
# Manually expand vector arrays with two elements into two separate vectors.
sed -i -E "s/^(\s*)(wire|reg)\s+(\[.*?\])\s+(buf_[adq][01])\s*(\[.*?\])\s*;/\1\2 \3 \4_0, \4_1;/g" fixed/all.v
# Then expand their assignments as well.
sed -i -E "s/\
 (buf_[adq][01])\[\s*([01])\s*\]/\
 \1_\2/g" fixed/all.v
# Insert a newline between consecutive ')' characters followed by a semicolon when the line starts with "."
# This simplifies the script that fixes port instantiation order.
sed -i -E 's/(\..*)\)\s*\);/\1\)\n\);/g' fixed/all.v
# Fix port order
# Recommend commenting this out until all other errors are resolved.
python3 fix_port_order.py fixed/all.v fixed/all.v
