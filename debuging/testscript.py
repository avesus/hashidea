import os, sys, subprocess

do_run=1
if len(sys.argv) == 2:
    do_run=int(sys.argv[1])

for i in range(0, do_run):
    print (str(i)+" done ")
    os.system('./debug_hashint 1 1024 32 3');
    os.system('./parse');


