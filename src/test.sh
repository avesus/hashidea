#!/bin/bash

trials=30
inserts=1600000
for table in hashtable_lazy_local hashtable_lazy hashtable_ll_rr hashtable_ll_tr hashtable_local; do
    /bin/rm harness
    make Hashtable=${table}.o WITHSTATS=0
    for t in 1 2 4 8; do
	#echo "Running for threads $t"
	for qp in 0 0.5 0.9 0.99; do
	    #echo "Running for qp $qp"
	    for it in $(( inserts * 2 )) $inserts 8192; do
		in=$(( inserts / t ))
		#echo "running with initial table size $it and $in inserts"
		echo ./harness --trials $trials --inserts $in --qp $qp -t $t -i $it
		./harness --trials $trials --inserts $in --qp $qp -t $t -i $it
	    done
	done
    done
done
	    
