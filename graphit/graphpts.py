#!/usr/bin/env python

# read in csv files of format below
# and produce graphs
#

import argparse
# import os
# import sys
import csv
# from pprint import pprint
import json

parser = argparse.ArgumentParser(description='take data cvs (from getstats.py) and make js file for graphing')
parser.add_argument("-v", "--verbosity", action="count", default=0, help="increase output verbosity")
parser.add_argument("datafile", nargs=1, help="csv file with data in it.")
args = vars(parser.parse_args())
infile = args['datafile'][0]

alpha = {i: 1 for i in ["main", "probe", "move", "version", "SP"]}
skip = ["version", "stopError", "randomSeed", "numInsertions"]
selectors = ["main", "probe", "move", "version", "trialsToRun", "stopError",
             "alpha", "beta", "queryPercentage", "randomSeed", "nthreads", "HashAttempts", "InitSize", "cooloff"]
sdata = {i: {} for i in selectors}
sindex = {}
controllers = []

print("{} => data.js, selectors.js".format(infile))

with open("data.js", "w") as outf:
    outf.write("window.data = [\n")
    with open(infile, "r") as inf:
        reader = csv.reader(inf)
        header = reader.next()
        for row in reader:
            outf.write("{},\n".format(json.dumps(row)))
        outf.write("];\n")

with open(infile, "r") as inf:
    reader = csv.DictReader(inf)
    for row in reader:
        for s in selectors:
            if row[s] not in sdata[s]:
                sdata[s][row[s]] = 0
            sdata[s][row[s]] += 1

with open("selectors.js", "w") as outf:
    outf.write("window.selector = {};\n")
    for s in selectors:
        if s in skip:
            continue
        outf.write("selector['{}'] = {};\n".format(s, json.dumps(sdata[s])))
        # check that there is more than one value before adding to controllers
        if len(sdata[s].keys()) > 1:
            controllers.append(s)

    outf.write("selector['index2name'] = {};\n".format(json.dumps(header)))
    headerdict = {}
    for i in range(len(header)):
        headerdict[header[i]] = i
    outf.write("selector['name2index'] = {};\n".format(json.dumps(headerdict)))
    outf.write("selector['controllers'] = {};\n".format(json.dumps(controllers)))
    name2number = {fld: 1 for fld in list(filter(lambda x: x not in alpha, header))}
    outf.write("selector['name2number'] = {};\n".format(json.dumps(name2number)))
