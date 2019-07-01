#!/bin/bash

trials=20
inserts=16000000
declare -a threads=(1 2 4 8 16)
declare -a queryp=(0 0.5 0.9)
declare -a attempts=(1 2 3 4)

d=`date -Iseconds`
exec > >(tee "$d.test.log") 2>&1

# set to true if you want to do a quick test
if [ 0 == 1 ]; then
    attempts=(1 3)
    threads=(1 2 4 16)
    queryp=(0 0.9)
    trials=5
    inserts=100
fi    

make clean
for table in hashtable_lazy_local hashtable_lazy hashtable hashtable_ll_tr hashtable_local; do
    /bin/rm -f harness
    d=`date`
    echo "---- Making with ${table} ---- ($d)"
    make Hashtable=${table}.o WITHSTATS=0
    if [ $? != 0 ]; then
	echo "Error:Failed to make harness for $table"
    else
	for t in ${threads[@]}; do
	    d=`date`
	    echo "Running for threads $t on table ${table} ($d)"
	    for qp in ${queryp[@]}; do
		#echo "Running for qp $qp"
		for it in $(( inserts * 2 )) 8192; do
		    for ha in ${attempts[@]}; do
			in=$(( inserts / t ))
			#echo "running with initial table size $it and $in inserts"
			echo ./harness --trials $trials --tracktemp  --inserts $in --qp $qp -t $t -i $it -a $ha --regtemp --args
			./harness --trials $trials --tracktemp --inserts $in --qp $qp -t $t -i $it -a $ha  --regtemp --args
			sleep 5
		    done
		done
	    done
	done
    fi
done
	    
