#!/bin/bash

declare -a threads=(1 2 4 8)
declare -a attempts=(1 2 3)

for table in hashtable_lazy_local hashtable_lazy hashtable hashtable_ll_tr hashtable_local; do
    /bin/rm -f harness
    make clean > /dev/null
    echo "---- Making with ${table} ----"
    make Hashtable=${table}.o WITHSTATS=0  > /dev/null
    if [ $? != 0 ]; then
	echo "Error:Failed to make harness for $table"
	exit
    else
	for t in ${threads[@]}; do
	    for ha in ${attempts[@]}; do
		echo -n "${table}: harness --trials 3 --tracktemp --inserts 1000 --qp 0.1 -t $t -i 2 -a $ha --regtemp "
		valgrind ./harness --trials 3 --tracktemp --inserts 1000 --qp 0.1 -t $t -i 2 -a $ha --regtemp 2>&1 | grep "ERROR SUMMARY"
		echo -n ""
	    done
	done
    fi
done
