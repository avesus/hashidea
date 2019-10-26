#!/bin/bash

#unless there is something I'm missing this is what I want to use
# perf stat -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations ./harness -i 10 --inserts 1000000 -t 8 --lines 1 -a 1


outFile=""
starttemp=56
trials=15
inserts=16000000
coreInterval=1
startCore=4
myExec="harness"
declare -a threads=(1 8 16 32)
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
if [ 1 == 0 ]; then
    attempts=(2)
    threads=(4)
    queryp=(.5)
    trials=5
    inserts=100000
fi

make clean;
for table in hashtable hashtable_cache hashtable_lazy_cache hashtable_lazy hashtable_cuckoo; do
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
    /bin/rm -f $myExec
    echo "---- Making with ${table} ---- ($d)"
    make Hashtable=${table}.o WITHSTATS=0
    if [ $? != 0 ]; then
	echo "Error:Failed to make $myExec for $table"
    else
	tablever=`./$myExec --version | sed -e 's/.*aka //'`
	for t in ${threads[@]}; do

	    endCore=$((startCore+t-1))
	    watchCores=""
	    for i in $(seq 0 $(($endCore-$startCore))); do val=$(($i*$coreInterval+$startCore)); watchCores="$watchCores$val,"; done
	    watchCores=${watchCores::-1}
	    echo "Running for threads $t on table ${table} ($d)"
	    for qp in ${queryp[@]}; do
		#echo "Running for qp $qp"
		for it in $((inserts)) $((inserts/4)); do
		    it=$(echo 'l('$it')/l(2)' | bc -l)
		    it=$(echo $it+.5 | bc)
		    it=${it%.*}
		    for ha in ${attempts[@]}; do
			for li in ${lines[@]}; do
			    in=$(( inserts / t ))
			    #echo "running with initial table size $it and $in inserts"
			    if [ $# -ne 0 ]; then
				./checklog.py --table ${tablever}  --inserts $in --qp $qp -t $t -i $it -a $ha --lines $li $prevlog
				doit=$?
			    else
				doit=1
			    fi
			    if [ $doit == 0 ]; then
				echo ALREADY  perf stat -x, -A --cpu=$watchCores -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations ./$myExec --trials $trials   --inserts $in --qp $qp -t $t -i $it -a $ha  --lines $li  --args --ci $coreInterval --sc $startCore --ec $endCore
			    else
				if [ $onlyshow -ne 1 ]; then
				    sleep 45
				    echo FIRST  perf stat -x=, -A --cpu=$watchCores -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations ./$myExec --trials $trials   --inserts $in --qp $qp -t $t -i $it -a $ha  --lines $li  --args --ci $coreInterval --sc $startCore --ec $endCore
				     perf stat -x=, -A --cpu=$watchCores -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations ./$myExec --trials $trials   --inserts $in --qp $qp -t $t -i $it -a $ha  --lines $li  --args --ci $coreInterval --sc $startCore --ec $endCore

				else
				    echo  perf stat -x=, -A --cpu=$watchCores -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations ./$myExec --trials $trials   --inserts $in --qp $qp -t $t -i $it -a $ha  --lines $li  --args --ci $coreInterval --sc $startCore --ec $endCore
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
