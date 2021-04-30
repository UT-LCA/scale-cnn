# Fixing Verilog for VTR

The Verilog compiler used by VTR, called ODIN, is very limited in its feature set and does not support a large number of commonly-used constructs in the Verilog language, such as for-generates, variable indexing of vectors, 1D arrays, low-to-high ranges, etc. Additionally, there are bugs in the compiler that can cause segfaults. This directory contains scripts that modify the HLS-generated code to avoid these problems while minimizing the behavioral changes to the code. Note: ODIN is actively being worked on and some of the issues and limitations I had previously may be fixed in the future.

Not all of the modifications are automated. Many are, but some still need to be made manually. See `steps.txt` for instructions on what manual steps need to be made, and how to use the scripts to fix the verilog. 

At the end of going through all these steps, the VTR-compatible Verilog code will be stored in `fixed/all.v`. This Verilog file can then be passed to ODIN and VTR.
