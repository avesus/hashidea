#!/bin/sh

sudo ./pcm.x -yc 1 
nthreads=8
ninserts=1000
ival=1
aval=2
toRun= ./pcm.x -yc 4 -yc 5 -yc 6 -yc 7 --external_program ./../src/harness -t $nthreads --inserts $ninserts -i $ival -a $aval --sc 4 --ec 7
