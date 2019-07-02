#!/usr/bin/env python

# read a log file and see if a particular run exists in it.

import argparse
import sys
# from pprint import pprint

verbose = False
objects = {'hashtable_lazy_local': 'O1L:V0.1/h:0.1',
           'hashtable_lazy': "OML:V0.1/h:0.1",
           'hashtable': "OMR:V0.1/h:0.1",
           'hashtable_ll_tr': "LSR:V0.1/h:0.1",
           'hashtable_local': 'O1R:V0.1/h:0.1'
           }


def obj2sp(name):
    if name not in objects:
        raise ValueError('cannot find {} in mapper from .o to SP'.format(name))
    return objects[name]


parser = argparse.ArgumentParser(description='read log file to check for good run')
# ./harness --trials 20 --inserts 1000000 --qp 0 -t 16 -i 32000000 -a 3
parser.add_argument("--table", type=str, help="table we are looking for", required=True)
parser.add_argument("--trials", type=int, help="how many trials we are searching for", required=True)
parser.add_argument("--inserts", type=int, help="how many inserts we are searching for", required=True)
parser.add_argument("--qp", type=float, help="query percentage we are searching for", required=True)
parser.add_argument("-t", "--threads", type=int, help="number of threads we are searching for", required=True)
parser.add_argument("-i", "--initialsize", type=int, help="initial hash table size we are searching for", required=True)
parser.add_argument("-a", "--hashattempts", type=int, help="hashattempts we are searching for", required=True)
parser.add_argument("logfiles", nargs='+', help="log file with run data")
flags = parser.parse_args()
check = {
    'SP': obj2sp(flags.table),
    'numInsertions': flags.inserts,
    'trialsToRun': flags.trials,
    'queryPercentage': flags.qp,
    'InitSize': flags.initialsize,
    'HashAttempts': flags.hashattempts,
    'nthreads': flags.threads
    }
types = {
    'SP': str,
    'numInsertions': int,
    'trialsToRun': int,
    'queryPercentage': float,
    'InitSize': int,
    'HashAttempts': int,
    'nthreads': int,
    }
for logfile in flags.logfiles:
    heading = []
    if verbose:
        print('Is --trials {} --inserts {} -qp {} -t {} -i {} -a {} in {}?'.
              format(flags.trials, flags.inserts, flags.qp, flags.threads,
                     flags.initialsize, flags.hashattempts, logfile))
    with open(logfile, "r") as f:
        for line in f:
            line = line.strip()
            if len(heading) == 0 and line.find(',HEADING') > 0:
                heading = line.strip().split(',')
                col2idx = {heading[i].strip(): i for i in range(0, len(heading))}
                continue
            if line.find(',END') < 0:
                continue
            # this represents a complete run
            data = line.strip().split(',')
            allok = True
            for fld in check:
                # print('{}={} => {}?{}'.format(fld, col2idx[fld], data[col2idx[fld]].strip(), check[fld]))
                x = data[col2idx[fld]].strip()
                if types[fld] != str:
                    if types[fld] == int:
                        x = int(x)
                    elif types[fld] == float:
                        x = float(x)
                if x != check[fld]:
                    allok = False
                    break
            if allok:
                sys.exit(0)
if verbose:
    print('Failed to find run')
sys.exit(1)
