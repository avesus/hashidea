#!/bin/bash

starttemp=56
trials=10
inserts=64000000
declare -a threads=(1 8 16 32 64)
declare -a queryp=(0 0.5 0.9)
declare -a attempts=(1 2)
declare -a lines=(.5 1 2)
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
    attempts=(2 3)
    threads=(1 2)
    lines=(1 .5 2)
    queryp=(0)
    trials=1
    inserts=128
fi    
#hashtable hashtable_lazy hashtable_locks hashtable_cuckoo
make clean
for table in hashtable_cache hashtable_lazy_cache ; do
    if [[ ($table == hashtable_cache) || ($table == hashtable_lazy_cache) ]]; then
	unset lines
	lines=(.5 1 2)
	unset attempts
	attempts=(1 2)
    else
	unset lines
	lines=(1)
	unset attempts
	attempts=(2)
    fi
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
		for it in $((inserts)) $((inserts/4)); do
		    it=$(echo 'l('$it')/l(2)' | bc -l)
		    it=$(echo $it+.5 | bc)
		    it=${it%.*}
		    for li in ${lines[@]}; do

			for ha in ${attempts[@]}; do
			    in=$(( inserts / t ))
			    #echo "running with initial table size $it and $in inserts"
			    if [ $# -ne 0 ]; then
				echo ./checklog.py --table ${tablever} --trials $trials --inserts $in --qp $qp -t $t -i $it -a $ha --lines $li $prevlog
				./checklog.py --table ${tablever} --trials $trials  --inserts $in --qp $qp -t $t -i $it -a $ha --lines $li $prevlog
				doit=$?
			    else
				doit=1
			    fi
			    if [ $doit == 0 ]; then
				echo ALREADY ./harness --trials $trials   --inserts $in --qp $qp -t $t -i $it -a $ha --lines $li  --args
			    else
				if [ $onlyshow -ne 1 ]; then
				    sleep 45
				    if [ $? == 1 ]; then
					echo "Failed to reach $starttemp, not running"
				    else
					echo ./harness --trials $trials   --inserts $in --qp $qp -t $t -i $it -a $ha --lines $li  --args
					./harness --trials $trials  --inserts $in --qp $qp -t $t -i $it -a $ha --lines $li   --args
				    fi
				else
				    echo ./harness --trials $trials   --inserts $in --qp $qp -t $t -i $it -a $ha --lines $li --args
				fi
			    fi
			done
		    done
		done
	    done
	done
    fi
done
echo "ALL DONE!"
