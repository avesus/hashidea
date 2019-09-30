#!/bin/bash


outFile=""
starttemp=56
trials=15
inserts=16000000
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
if [ 1 == 1 ]; then
    attempts=(1)
    threads=(4)
    queryp=(.5)
    trials=5
    inserts=100000
fi

make clean
for table in hashtable_cache hashtable_lazy_cache hashtable hashtable_lazy hashtable_locks hashtable_cuckoo; do
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
    newDir=${table}_pcmOut
    if [ -d "$newDir" ]; then
	rm -f "$newDir"/*
    else
	mkdir -p "$newDir"
    fi
    d=`date`
    echo "---- Making with ${table} ---- ($d)"
    make Hashtable=${table}.o WITHSTATS=0
    if [ $? != 0 ]; then
	echo "Error:Failed to make harness for $table"
    else
	tablever=`./harness --version | sed -e 's/.*aka //'`
	for t in ${threads[@]}; do
	    startCore=4
	    endCore=$((startCore+t-1))
	    pcmCores=""
	    for i in $(seq $startCore $endCore); do pcmCores="$pcmCores -yc $i"; done
	    d=`date`
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
				outFile=`date -Iseconds`
				echo ALREADY ./../pcm/pcm.x -csv=$outFile $pcmCores --external_program ./harness --trials $trials --tracktemp  --inserts $in --qp $qp -t $t -i $it -a $ha  --lines $li --regtemp --args --sc $startCore --ec $endCore
			    else
				if [ $onlyshow -ne 1 ]; then
				    sleep 45
				    outFile=`date -Iseconds`
				    outFile=${newDir}/${outFile}
				    echo FIRST ./../pcm/pcm.x -csv=$outFile $pcmCores --external_program ./harness --trials $trials --tracktemp  --inserts $in --qp $qp -t $t -i $it -a $ha  --lines $li --regtemp --args --sc $startCore --ec $endCore
				    sudo modprobe msr
				    ./../pcm/pcm.x -csv=$outFile $pcmCores --external_program ./harness --trials $trials --tracktemp --inserts $in --qp $qp -t $t -i $it -a $ha --lines $li --regtemp --args --sc $startCore --ec $endCore
				    echo -e "" >> $outFile
				    echo -e "" >> $outFile
				    echo UNIQUEMARKER ./../pcm/pcm.x -csv=$outFile $pcmCores --external_program ./harness --trials $trials --tracktemp  --inserts $in --qp $qp -t $t -i $it -a $ha  --lines $li --regtemp --args --sc $startCore --ec $endCore >> $outFile
				    #					done
				fi
			    else
				echo ./../pcm/pcm.x -csv=$outFile $pcmCores --external_program ./harness --trials $trials --tracktemp  --inserts $in --qp $qp -t $t -i $it -a $ha  --lines $li --regtemp --args --sc $startCore --ec $endCore
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
