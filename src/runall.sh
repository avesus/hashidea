!#/bin/bash
arg1=$1 #trials
arg2=$2 #threads
arg3=$3 #initial value
arg4=$4 #hash functions
arg5=$5 #inserts
if [ -z "$1" ]
  then
      arg1=4
fi
if [ -z "$2" ]
  then
      arg2=8
fi
if [ -z "$3" ]
  then
      arg3=1048576
fi
if [ -z "$4" ]
  then
      arg4=2
fi
if [ -z "$5" ]
  then
      arg5=2000000
fi
make clean; make;
for f in ../lib/hashtable*.o; do h="$f"; echo "make clean; make Hashtable=${h:7}"; make clean; make Hashtable=${h:7}; echo "${h:7} ---- START  ---- ${h:7}"; echo "./harness -T --trials $arg1 -t $arg2 -i $arg3 -a $arg4 --inserts $arg5"; ./harness -T --trials $arg1 -t $arg2 -i $arg3 -a $arg4 --inserts $arg5 ; echo "${h:7} ---- END  ---- ${h:7}"; sleep 1; done;
