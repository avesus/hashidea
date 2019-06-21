#!/usr/bin/env python

import argparse
# import subprocess
import re
from pprint import pprint
import sys


################################################################
# setup arguments
parser = argparse.ArgumentParser(description='generate graphs from harness data')
parser.add_argument("-v", "--verbosity", action="count", default=0, help="increase output verbosity")
parser.add_argument("-o", "--out", default="", help="output file, defaults to stdout")
parser.add_argument("datafile", nargs=1, help="file with data in it.")
# parse arguments
flags = parser.parse_args()
verbose = flags.verbosity
inname = flags.datafile[0]
outname = flags.out
if outname == "":
    outfile = sys.stdout
else:
    outfile = open(outname, "w")

state = None
heading = []
hasheading = {}
result = []
with open(inname, "r") as infile:
    for line in infile:
        line = line.strip()
        args = [x.strip() for x in line.split(",")]
        last = args[len(args)-1]
        if state is None:
            if last == 'START':
                state = last
                hashtype = args[0].split(':')[0].split("/")
                hashtype.append(args[0].split(':')[1])
                prefix = hashtype
                prefix.extend(args[1:-1])
                elimprefix = len(line.split(","))-1
                outdata = prefix
            elif len(heading) == 0 and last == 'HEADING':
                heading = args[1:-1]
                hprefix = ['main', 'probe', 'move', 'version']
                hprefix.extend(heading)
                heading = hprefix
        elif state == 'START':
            args = args[elimprefix:]
            if last == 'END':
                state = None
                # print("{}\n".format(",".join(outdata)))
                result.append(outdata)
                outdata = []
            kind = args[0]
            args = args[1:]
            if kind not in hasheading:
                hasheading[kind] = 1
                tags = ["-".join([kind, x.split(':')[0]]) for x in args]
                heading.extend(tags)
            outdata.extend([x.split(':')[1] for x in args])

outfile.write("{}\n".format(",".join(heading)))
for line in result:
    outfile.write("{}\n".format(",".join(line)))
