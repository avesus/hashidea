import sys
import statistics
import math

f = sys.argv[1]
includeCpu = 0
state = 0
ncpu = 0
curCpu = 0
tnum = 0
fullList = [[[[
    [] for z in range(2)]
              for x in range(33)]
             for y in range(33)]
            for h in range(6)]
modified = []
tmodified = []
tnames = [
    "hashtable",
    "hashtable_cache_np",
    "hashtable_lazy",
    "hashtable_lazy_cache_np",
    "hashtable_cuckoo"
]


def tToIndex(tname):
    if tname == "hashtable":
        return 0
    elif tname == "hashtable_cache_np":
        return 1
    elif tname == "hashtable_lazy":
        return 2
    elif tname == "hashtable_lazy_cache_np":
        return 3
    elif tname == "hashtable_cuckoo":
        return 4


def iToName(index):
    return tnames[index]

doTable = sys.argv[2]
run = ""
for i in range(3, len(sys.argv)):
    run = run + " " + sys.argv[i]
print("Run:" + run)
for i in range(0, 5):
    tmodified.append(0)
for i in range(0, 33):
    modified.append(0)
for row in open(f):
    if state == 0:
        if "Making" in row:
            srow = row.split()
            tnum = tToIndex(srow[3])
            if srow[3] == doTable:
                tmodified[tnum] = 1
    if state == 0:
        if "FIRST" in row:
            if doTable != iToName(tnum):
                continue
            myCont = 0
            if run not in row:
                myCont = 1
            if myCont == 1:
                continue
            ncpu = 0
            curCpu = 0
            waste1, waste2 = row.split('--cpu=')
            rel, waste1 = waste2.split('-e L1')
            for i in range(0, len(rel)):
                if rel[i] == ',':
                    ncpu = ncpu+1
            ncpu = ncpu + 1
            modified[ncpu] = 1
            state = 1
    elif state == 1:
        if "cache-loads" in row:
            rowRAM = row.split()
            valArr = rowRAM[1].split(',')
            val = ""
            for j in range(0, len(valArr)):
                val = val + valArr[j]

            if val == "" or len(val) == 0:
                print("Error in val: " + val)
                exit(0)
            if float(val) == 0:
                print("Error in float val: " + val)
                exit(0)
            fullList[tnum][ncpu][curCpu][0].append(val)
            curCpu = curCpu + 1
            if curCpu == ncpu:
                curCpu = 0
        if "cache-misses" in row:
            rowRAM = row.split()
            valArr = rowRAM[1].split(',')
            val = ""
            for j in range(0, len(valArr)):
                val = val + valArr[j]
            if val == "" or len(val) == 0:
                print("Error in val: " + val)
                exit(0)
            if float(val) == 0:
                print("Error in float val: " + val)
                exit(0)
            fullList[tnum][ncpu][curCpu][1].append(val)
            curCpu = curCpu + 1
            if curCpu == ncpu:
                state = 0
for t in range(0, 5):
    if tmodified[t] == 0:
        continue
    print("---------------Doing Table: " + iToName(t) + "---------------")
    tstatArrLoads = []
    tstatArrMisses = []
    tstatArrRatio = []
    for i in range(0, 33):
        if modified[i]:
            statArrLoads = []
            statArrMisses = []
            statArrRatio = []
            for j in range(0, i):
                lstatArrLoads = []
                lstatArrMisses = []
                lstatArrRatio = []
                if includeCpu != 0:
                    print("----------------CPU" + str(j) + "----------------")
                if len(fullList[t][i][j][0]) != len(fullList[t][i][j][1]):
                        print("Error in getting values!")
                        exit(0)
                for k in range(0, len(fullList[t][i][j][0])):
                    lstatArrLoads.append(float(fullList[t][i][j][0][k]))
                    lstatArrMisses.append(float(fullList[t][i][j][1][k]))
                    lstatArrRatio.append(
                        100*(float(fullList[t][i][j][1][k])/float(fullList[t][i][j][0][k]))
                    )
                    statArrLoads.append(float(fullList[t][i][j][0][k]))
                    statArrMisses.append(float(fullList[t][i][j][1][k]))
                    statArrRatio.append(
                        100*(float(fullList[t][i][j][1][k])/float(fullList[t][i][j][0][k]))
                    )
                    tstatArrLoads.append(float(fullList[t][i][j][0][k]))
                    tstatArrMisses.append(float(fullList[t][i][j][1][k]))
                    tstatArrRatio.append(
                        100*(float(fullList[t][i][j][1][k])/float(fullList[t][i][j][0][k]))
                    )
                if includeCpu != 0:
                    print("Loads Avg:  " + str(round(statistics.mean(lstatArrLoads), 2)),
                          end=' ')
                    print("Loads Med:  " + str(round(statistics.median(lstatArrLoads), 2)),
                          end=' ')
                    if(len(lstatArrLoads) > 1):
                        print("Loads Std:  " + str(round(statistics.stdev(lstatArrLoads), 2)))
                    else:
                        print("")
                    print("Misses Avg: " + str(round(statistics.mean(lstatArrMisses), 2)),
                          end=' ')
                    print("Misses Med: " + str(round(statistics.median(lstatArrMisses), 2)),
                          end=' ')
                    if(len(lstatArrMisses) > 1):
                        print("Misses Std: " + str(round(statistics.stdev(lstatArrMisses), 2)))
                    else:
                        print("")
                    print("Ratio Avg:  " + str(round(statistics.mean(lstatArrRatio), 4)),
                          end=' ')
                    print("Ratio Med: " + str(round(statistics.median(lstatArrRatio), 4)),
                          end=' ')
                    if(len(lstatArrRatio) > 1):
                        print("Ratio Std: " + str(round(statistics.stdev(lstatArrRatio), 4)))
                    else:
                        print("")
                    if j > 9:
                        print("-", end='')
                    print("------------------------------------")
                    
            print("Misses Avg: " + str(round(statistics.mean(statArrMisses), 2)) + " == 10^" + str(int(math.log(round(statistics.mean(statArrMisses), 2),10))) , end='\n')
            print("Loads Avg : " + str(round(statistics.mean(statArrLoads), 2)) + " == 10^" + str(int(math.log(round(statistics.mean(statArrLoads), 2),10))), end='\n')
            print("Misses Med: " + str(round(statistics.median(statArrMisses), 2)) + " == 10^" + str(int(math.log(round(statistics.median(statArrMisses), 2),10))), end='\n')
            print("Loads Med : " + str(round(statistics.median(statArrLoads), 2)) + " == 10^" + str(int(math.log(round(statistics.median(statArrLoads), 2),10))), end='\n')
            if(len(statArrMisses) > 1):
                print("Misses Std: " + str(round(statistics.stdev(statArrMisses) , 2)) + " == 10^" + str(int(math.log(round(statistics.stdev(statArrMisses), 2),10))))
            if(len(statArrLoads) > 1):
                print("Loads Std : " + str(round(statistics.stdev(statArrLoads), 2)) + " == 10^" + str(int(math.log(round(statistics.stdev(statArrLoads), 2),10))))
            print("Ratio Avg : " + str(round(statistics.mean(statArrRatio), 4)) + "%", end='\n')
            print("Ratio Med : " + str(round(statistics.median(statArrRatio), 4)) + "%", end='\n')
            if(len(statArrRatio) > 1):
                print("Ratio Std : " + str(round(statistics.stdev(statArrRatio), 4)) + "%")
            print("------------------------------------------------------------")
