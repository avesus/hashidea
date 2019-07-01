#!/bin/bash

trials=20
inserts=16000000

make clean
for table in hashtable_lazy_local hashtable_lazy hashtable hashtable_ll_tr hashtable_local; do
    /bin/rm -f harness
    d=`date`
    echo "---- Making with ${table} ---- ($d)"
    make Hashtable=${table}.o WITHSTATS=0
    if [ $? != 0 ]; then
	echo "Error:Failed to make harness for $table"
    else
	for t in 1 2 4 8 16; do
	    d=`date`
	    echo "Running for threads $t ($d)"
	    for qp in 0 0.5 0.9; do
		#echo "Running for qp $qp"
		for it in $(( inserts * 2 )) 8192; do
		    for ha in {1..4}; do
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
	    
