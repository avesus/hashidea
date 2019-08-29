#!/bin/bash


for i in 1 2 3 4; do for j in .25 .5. 1 2 4; do echo $i $j; for k in {0..0}; do ./harness --inserts 100000 -t 1 --trials 1000 -i 1 -a $i --lines $j --dp 1 --maxVal 1000; done; sleep 2; done; done;
