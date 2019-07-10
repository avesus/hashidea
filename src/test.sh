#!/bin/bash

starttemp=56
trials=15
inserts=16000000
declare -a threads=(1 2 4 8 16 32)
declare -a queryp=(0 0.5 0.9)
declare -a attempts=(1 2 3)

onlyshow=0
prevlog=$*
d=`date -Iseconds`
if [ $onlyshow -ne 1 ]; then
    exec > >(tee "$d.test.log") 2>&1
fi
if [ $# -ne 0 ]; then
    echo "Running tests while skipping any already preformed in the following logs: $prevlog"
fi

# set to true if you want to do a quick test
if [ 0 == 1 ]; then
    attempts=(1 3)
    threads=(1 2 4 8 16 32)
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
	tablever=`./harness --version | sed -e 's/.*aka //'`
	for t in ${threads[@]}; do
	    d=`date`
	    echo "Running for threads $t on table ${table} ($d)"
	    for qp in ${queryp[@]}; do
		#echo "Running for qp $qp"
		for it in $((2*inserts)) $(($inserts)) $((inserts/2)) $((inserts/4)); do 
		    for ha in ${attempts[@]}; do
			in=$(( inserts / t ))
			#echo "running with initial table size $it and $in inserts"
			if [ $# -ne 0 ]; then
			    ./checklog.py --table ${tablever}  --inserts $in --qp $qp -t $t -i $it -a $ha $prevlog
			    doit=$?
			else
			    doit=1
			fi
			if [ $doit == 0 ]; then
			    echo ALREADY ./harness --trials $trials --tracktemp  --inserts $in --qp $qp -t $t -i $it -a $ha --regtemp --args
			else
			    if [ $onlyshow -ne 1 ]; then
				./waitfortemp -t 600 -n $t $starttemp
				if [ $? == 1 ]; then
				    echo "Failed to reach $starttemp, not running"
				else
				    echo ./harness --trials $trials --tracktemp  --inserts $in --qp $qp -t $t -i $it -a $ha --regtemp --args
				    ./harness --trials $trials --tracktemp --inserts $in --qp $qp -t $t -i $it -a $ha  --regtemp --args
				fi
			    else
				echo ./harness --trials $trials --tracktemp  --inserts $in --qp $qp -t $t -i $it -a $ha --regtemp --args
			    fi
			fi
		    done
		done
	    done
	done
    fi
done
echo "ALL DONE!"
