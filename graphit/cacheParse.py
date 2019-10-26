import sys
import statistics
f = sys.argv[1]

state = 0
ncpu = 0
curCpu = 0
tnum = 0
fullList = [[[[] for x in range(33)] for y in range(33)] for h in range(6)]
modified = []
tnames = ["hashtable", "hashtable_cache", "hashtable_cuckoo", "hashtable_lazy", "hashtable_lazy_cache"]
def tToIndex(tname):
    if tname == "hashtable":
        return 0
    elif tname == "hashtable_cache":
        return 1
    elif tname == "hashtable_lazy":
        return 2
    elif tname == "hashtable_lazy_cache":
        return 3
    elif tname == "hashtable_cuckoo":
        return 4


def iToName(index):
    return tnames[index]
    
for i in range(0, 33):
    modified.append(0)
for row in open(f):
    if state == 0:
        if "Making" in row:
            srow = row.split()
            tnum = tToIndex(srow[3])
    if state == 0:
        if "FIRST" in row:
            ncpu = 0
            curCpu = 0
            waste1, waste2 = row.split('--cpu=')
            rel, waste1 = waste2.split('-e cache')
            for i in range(0, len(rel)):
                if rel[i] == ',':
                    ncpu = ncpu+1
            ncpu = ncpu + 1
            modified[ncpu] = 1
            state = 1
    elif state == 1:
        if "cache-misses" in row:
            rowRAM = row.split(',')
            fullList[tnum][ncpu][curCpu].append(rowRAM[6][:len(rowRAM[6])-2])
            curCpu = curCpu + 1
            if curCpu == ncpu:
                state = 0
for t in range(0, 5):
    print("Doing Table: " + iToName(t))
    tstatArr = []
    for i in range(0, 33):
        if modified[i]:
            statArr = []
            print("Doing " + str(i) + " CPUs")
            for j in range(0, i):
                lstatArr = []
                print("CPU" + str(j) + ": ", end='')
                for k in range(0, len(fullList[t][i][j])-1):
                    lstatArr.append(float(fullList[t][i][j][k]))
                    statArr.append(float(fullList[t][i][j][k]))
                    tstatArr.append(float(fullList[t][i][j][k]))
                lstatArr.append(float(fullList[t][i][j][len(fullList[t][i][j])-1]))
                statArr.append(float(fullList[t][i][j][len(fullList[t][i][j])-1]))
                tstatArr.append(float(fullList[t][i][j][len(fullList[t][i][j])-1]))
                print("Avg: " + str(round(statistics.mean(lstatArr), 2)), end=' ')
                print("Med: " + str(round(statistics.median(lstatArr), 2)), end=' ')
                print("Std: " + str(round(statistics.stdev(lstatArr), 2)))
            print("Avg: " + str(round(statistics.mean(statArr), 2)), end=' ')
            print("Med: " + str(round(statistics.median(statArr), 2)), end=' ')
            print("Std: " + str(round(statistics.stdev(statArr), 2)))
