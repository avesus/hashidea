#!/usr/bin/env python

import argparse
import re
# import subprocess
from pprint import pprint
import sys


def resetState(args):
    global state, elimprefix, outdata, perfdata, perfheader
    state = 'START'
    hashtype = args[0].split(':')[0].split("/")
    hashtype.append(args[0].split(':')[1])
    prefix = hashtype
    prefix.extend(args[1:-1])
    elimprefix = len(line.split(","))-1
    outdata = prefix
    perfdata = {}
    perfheader = {}


def parseerror(msg, dieflag=True):
    sys.stderr.write('{} on {}:{}\n'.format(msg, inname, ln))
    if windowsize > 0:
        sys.stderr.write('{}\n'.format("\n".join(window[-windowsize:])))
    if dieflag:
        sys.exit(-1)


# get avg, median, min, max, and Std Dev of data in array
def calcPerfStats(data):
    return [str(data[0]), str(data[0]), str(data[0]), str(data[0]), str(0)]


################################################################
# setup arguments
parser = argparse.ArgumentParser(description='generate graphs from harness data')
parser.add_argument("-v", "--verbosity", action="count", default=0, help="increase output verbosity")
parser.add_argument("-o", "--out", default="", help="output file, defaults to stdout")
parser.add_argument("-p", "--perfstat", action="store_true", help="if set, then include perfstat data too")
parser.add_argument("-w", "--window", type=int, default=0, help="prev window lines shown on error")
parser.add_argument("datafile", nargs="+", help="file with data in it.")
# parse arguments
flags = parser.parse_args()
verbose = flags.verbosity
inname = flags.datafile[0]
windowsize = flags.window
outname = flags.out
getperfstat = flags.perfstat
if outname == "":
    outfile = sys.stdout
else:
    outfile = open(outname, "w")

# state will be None, START, END, FINDPERF (the last only if getperfstat is true), GETPERF, WRITE
state = None
perfState = "FINDPERF" if getperfstat else "WRITE"
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
            if len(line) == 0:
                # skip blank lines
                continue
            window.append(line)
            args = [x.strip() for x in line.split(",")]
            print("{}:{}:{}".format(ln, state, line))
            # get tag from end of input line.  important ones are START, END, HEADING
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
                    # got last of data from the run being processed.
                    # If we are getting perfstat info, then we get
                    # that now.
                    state = perfState
                    # print("{}\n".format(",".join(outdata)))
                elif last == 'START':
                    parseerror('Missing end, 2 STARTS?', dieflag=False)
                    resetState(oldargs)
                elif len(args) > 0:
                    # this input line has data to collect
                    kind = args[0]
                    args = args[1:]
                    if kind not in hasheading:
                        hasheading[kind] = 1
                        tags = ["-".join([kind, x.split(':')[0]]) for x in args]
                        # print('Extending hashheading:{} {} {}'.format(ln, kind, ",".join(tags)))
                        heading.extend(tags)
                    outdata.extend([x.split(':')[1] for x in args])
            elif state == 'FINDPERF':
                # we are waiting for perf data
                if line.startswith("Performance counter stats for"):
                    # get number of cpus
                    match = re.search('CPU.s. ([0-9,]+)', line)
                    if match is None:
                        parseerror('Missing CPU spec')
                    cpus = match.group(1).split(",")
                    state = 'GETPERF'
                else:
                    parseerror('Expected line starting with "Performance counter ..."')
            elif state == 'GETPERF':
                if line.find("seconds time elapsed") > 0:
                    # calculate min, max, avg, median, and SD for each field in perfheader
                    for key in sorted(perfheader):
                        if key not in hasheading:
                            hasheading[key] = 1
                            heading.extend(["-".join([key, x]) for x in ["min", "max", "avg", "med", "SD"]])
                        cnts = []
                        for cpu in perfdata:
                            cnts.append(perfdata[cpu][key])
                        outdata.extend(calcPerfStats(cnts))
                    # we are done with this run, so write out data
                    state = 'WRITE'
                else:
                    args = re.sub('[ \t]+', ' ', line.strip().replace('CPU', '').replace(',', '')).split(" ")
                    pprint(args)
                    if args[0] not in perfdata:
                        perfdata[args[0]] = {}
                    if args[2] not in perfheader:
                        perfheader[args[2]] = 1
                    if args[2] not in perfdata[args[0]]:
                        perfdata[args[0]][args[2]] = args[1]

            # write out data if requested
            if state == 'WRITE':
                # write out data we collected for this run, and reset data collection
                result.append(outdata)
                outdata = []
                state = None


outfile.write("{}\n".format(",".join(heading)))
for line in result:
    outfile.write("{}\n".format(",".join(line)))
