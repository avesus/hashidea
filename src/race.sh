#!/bin/sh


for i in 1 2 3 4; do for j in .25 .5. .75 1 1.25 1.5 1.75 2 2.25 3 3.5; do for k in 1 8 64 128; do echo $i $j $k; ./harness --inserts 3000 -t 8 --trials 500 -i 1 -a $i --lines $j; sleep 3; done; done; done;
