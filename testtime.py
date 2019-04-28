import os, sys, subprocess

do_run=1
do_int=0
if len(sys.argv) == 3:
    do_run=int(sys.argv[1])
    do_int=int(sys.argv[2])
inits=[1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768, 65536,131072,262144,524288,1048576,2097152,4194304,8388608,16777216,33554432]
runs=[1024,2048,4096,8192,16384,32768, 65536,131072,262144,524288,1048576,2097152,4194304]
num_threads = [1,2,4,8,16,32]

count=0
for i in range(0, do_run):
    for j in range(0, 10):
        count=count+1
        size=inits[i%len(inits)]
        run=runs[i%len(runs)]
        num=32 #num_threads[i%len(num_threads)]
        if do_int == 1:
            print(str(count)+ ') starting hashint ' + str(size) + ' ' + str(run)+ ' ' + str(num));
            os.system('echo --------- START '+ str(count)+' ------------- >> intnb.txt');
            os.system('echo hashint ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ' >> intnb.txt');
            os.system('(time ./hashint ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ') >> intnb.txt 2>&1');
            os.system('echo --------- END '+ str(count)+' ------------- >> intnb.txt');
            if size<run*num:
                size=run*num
            print(str(count)+ ') starting lhi ' + str(size) + ' ' + str(run)+ ' ' + str(num));
            os.system('echo --------- START '+ str(count)+' ------------- >> intlock.txt');
            os.system('echo lhi ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ' >> intlock.txt');
            os.system('(time ./lhi ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ') >> intlock.txt 2>&1');
            os.system('echo --------- END '+ str(count)+' ------------- >> intlock.txt');
        else:
            print(str(count)+ ') starting hash ' + str(size) + ' ' + str(run)+ ' ' + str(num));
            os.system('echo --------- START '+ str(count)+' ------------- >> strnb.txt');
            os.system('echo hash ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ' >> strnb.txt');
            os.system('(time ./hash ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ') >> strnb.txt 2>&1');
            os.system('echo --------- END '+ str(count)+' ------------- >> strnb.txt');
            if size<run*num:
                size=run*num
            print(str(count)+ ') starting lh ' + str(size) + ' ' + str(run)+ ' ' + str(num));
            os.system('echo --------- START '+ str(count)+' ------------- >> strlock.txt');
            os.system('echo lh ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ' >> strlock.txt');
            os.system('(time ./lh ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ') >> strlock.txt 2>&1');
            os.system('echo --------- END '+ str(count)+' ------------- >> strlock.txt');
