import os, sys, subprocess

do_run=1
do_int=0

inits=[1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768, 65536,131072,262144,524288,1048576,2097152,4194304,8388608,16777216,33554432]
runs=[1024,2048,4096,8192,16384,32768, 65536,131072,262144,524288,1048576,2097152,4194304]
num_threads = [1,2,4,8,16,32]
jmax =100
imax =5
count=0
for i in range(0, imax):
    for j in range(0, jmax):
        count=count+1
        size=1
        run=262144*32
        vecs=3#+4*i
        num=1 #num_threads[i%len(num_threads)]
        
        print(str(count)+ ') starting hashint ' + str(size) + ' ' + str(run) + ' ' + str(num) +' '+ str(vecs));
        os.system('echo --------- START '+ str(count)+' ------------- >> debug.txt');
        os.system('echo hashint ' + str(size) + ' ' + str(run)+ ' ' + str(num) +' '+str(vecs)+ ' >> debug.txt');
        os.system('(time ./hashint ' + str(size) + ' ' + str(run)+ ' ' + str(num) +' '+str(vecs)+ ') >> debug.txt 2>&1');
        os.system('echo --------- END '+ str(count)+' ------------- >> debug.txt');

#count=800

exit(0)
for i in range(3, imax):
    for j in range(0, jmax):
        count=count+1
        size=1048576
        run=1048576
        vecs=3+4*i
        num=32 #num_threads[i%len(num_threads)]
        print(str(count)+ ') starting hashint ' + str(size) + ' ' + str(run)+ ' ' + str(num))+' '+str(vecs);
        os.system('echo --------- START '+ str(count)+' ------------- >> data2.txt');
        os.system('echo hashint ' + str(size) + ' ' + str(run)+ ' ' + str(num) +' '+str(vecs)+ ' >> data2.txt');
        os.system('(time ./hashint ' + str(size) + ' ' + str(run)+ ' ' + str(num) +' '+str(vecs)+ ') >> data2.txt 2>&1');
        os.system('echo --------- END '+ str(count)+' ------------- >> data2.txt');



for i in range(0, imax):
    for j in range(0, jmax):
        count=count+1
        size=1
        run=262144
        vecs=3+4*i
        num=32 #num_threads[i%len(num_threads)]
        
        print(str(count)+ ') starting hashint2 ' + str(size) + ' ' + str(run)+ ' ' + str(num))+' '+str(vecs);
        os.system('echo --------- START '+ str(count)+' ------------- >> data3.txt');
        os.system('echo hashint2 ' + str(size) + ' ' + str(run)+ ' ' + str(num) +' '+str(vecs)+ ' >> data3.txt');
        os.system('(time ./hashint2 ' + str(size) + ' ' + str(run)+ ' ' + str(num) +' '+str(vecs)+ ') >> data3.txt 2>&1');
        os.system('echo --------- END '+ str(count)+' ------------- >> data3.txt');
for i in range(0, imax):
    for j in range(0, jmax):
        count=count+1
        size=1048576
        run=1048576
        vecs=3+4*i
        num=32 #num_threads[i%len(num_threads)]
        print(str(count)+ ') starting hashint2 ' + str(size) + ' ' + str(run)+ ' ' + str(num))+' '+str(vecs);
        os.system('echo --------- START '+ str(count)+' ------------- >> data4.txt');
        os.system('echo hashint2 ' + str(size) + ' ' + str(run)+ ' ' + str(num) +' '+str(vecs)+ ' >> data4.txt');
        os.system('(time ./hashint2 ' + str(size) + ' ' + str(run)+ ' ' + str(num) +' '+str(vecs)+ ') >> data4.txt 2>&1');
        os.system('echo --------- END '+ str(count)+' ------------- >> data4.txt');



#            if size<run*num:
 #               size=run*num
  #          print(str(count)+ ') starting lhi ' + str(size) + ' ' + str(run)+ ' ' + str(num));
   #         os.system('echo --------- START '+ str(count)+' ------------- >> intlock.txt');
    #        os.system('echo lhi ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ' >> intlock.txt');
     #       os.system('(time ./lhi ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ') >> intlock.txt 2>&1');
      #      os.system('echo --------- END '+ str(count)+' ------------- >> intlock.txt');
      #  else:
       #     print(str(count)+ ') starting hash ' + str(size) + ' ' + str(run)+ ' ' + str(num));
        #    os.system('echo --------- START '+ str(count)+' ------------- >> strnb.txt');
         #   os.system('echo hash ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ' >> strnb.txt');
         #   os.system('(time ./hash ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ') >> strnb.txt 2>&1');
          #  os.system('echo --------- END '+ str(count)+' ------------- >> strnb.txt');
          #  if size<run*num:
          #      size=run*num
          #  print(str(count)+ ') starting lh ' + str(size) + ' ' + str(run)+ ' ' + str(num));
          #  os.system('echo --------- START '+ str(count)+' ------------- >> strlock.txt');
          #  os.system('echo lh ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ' >> strlock.txt');
          #  os.system('(time ./lh ' + str(size) + ' ' + str(run)+ ' ' + str(num) + ') >> strlock.txt 2>&1');
        #    os.system('echo --------- END '+ str(count)+' ------------- >> strlock.txt');
