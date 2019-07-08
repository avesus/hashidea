#!/usr/bin/env python

import argparse
# import subprocess
from pprint import pprint
import sys


def resetState(args):
    global state, elimprefix, outdata
    state = 'START'
    hashtype = args[0].split(':')[0].split("/")
    hashtype.append(args[0].split(':')[1])
    prefix = hashtype
    prefix.extend(args[1:-1])
    elimprefix = len(line.split(","))-1
    outdata = prefix


################################################################
# setup arguments
parser = argparse.ArgumentParser(description='generate graphs from harness data')
parser.add_argument("-v", "--verbosity", action="count", default=0, help="increase output verbosity")
parser.add_argument("-o", "--out", default="", help="output file, defaults to stdout")
parser.add_argument("-w", "--window", type=int, default=0, help="prev window lines shown on error")
parser.add_argument("datafile", nargs="+", help="file with data in it.")
# parse arguments
flags = parser.parse_args()
verbose = flags.verbosity
inname = flags.datafile[0]
windowsize = flags.window
outname = flags.out
if outname == "":
    outfile = sys.stdout
else:
    outfile = open(outname, "w")

state = None
heading = []
hasheading = {}
result = []
for inname in flags.datafile:
    sys.stderr.write("{}\n".format(inname))
    window = []
    with open(inname, "r") as infile:
        ln = 0
        sys.stderr.write("{}:{}\r".format(inname, ln))
        for line in infile:
            ln += 1
            line = line.strip()
            window.append(line)
            args = [x.strip() for x in line.split(",")]
            last = args[len(args)-1]
            # pprint(args)
            if state is None:
                if last == 'START':
                    resetState(args)
                elif len(heading) == 0 and last == 'HEADING':
                    heading = args[1:-1]
                    hprefix = ['main', 'probe', 'move', 'version']
                    hprefix.extend(heading)
                    heading = hprefix
            elif state == 'START':
                if verbose:
                    print('{}:{}:{}'.format(ln, last, line))
                oldargs = args
                args = args[elimprefix:]
                if last == 'END':
                    state = None
                    # print("{}\n".format(",".join(outdata)))
                    result.append(outdata)
                    outdata = []
                elif last == 'START':
                    sys.stderr.write('Missing end, 2 STARTS? {}:{}\n'.format(inname, ln))
                    if windowsize > 0:
                        sys.stderr.write('{}\n'.format("\n".join(window[-windowsize:])))
                    resetState(oldargs)
                elif len(args) > 0:
                    kind = args[0]
                    args = args[1:]
                    if kind not in hasheading:
                        hasheading[kind] = 1
                        tags = ["-".join([kind, x.split(':')[0]]) for x in args]
                        # print('Extending hashheading:{} {} {}'.format(ln, kind, ",".join(tags)))
                        heading.extend(tags)
                    outdata.extend([x.split(':')[1] for x in args])

outfile.write("{}\n".format(",".join(heading)))
for line in result:
    outfile.write("{}\n".format(",".join(line)))
