#!/bin/bash

items=8388608
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -c -r 10
done;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -r 10
done;
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -c -r 10 -q 50
done;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -r 10 -q 50
done;
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -c -r 10 -q 90
done;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -r 10 -q 90
done;
