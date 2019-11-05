#!/bin/bash

items=16777216
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -c -r 10
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -r 10
done;
sleep 20;
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -c -r 10 -q 50
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -r 10 -q 50
done;
sleep 20;
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -c -r 10 -q 90
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 23 -n $ins -t $i -r 10 -q 90
done;
sleep 20;


for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 24 -n $ins -t $i -c -r 10
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 24 -n $ins -t $i -r 10
done;
sleep 20;
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 24 -n $ins -t $i -c -r 10 -q 50
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 24 -n $ins -t $i -r 10 -q 50
done;
sleep 20;
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 24 -n $ins -t $i -c -r 10 -q 90
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 24 -n $ins -t $i -r 10 -q 90
done;
sleep 20;


for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 25 -n $ins -t $i -c -r 10
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 25 -n $ins -t $i -r 10
done;
sleep 20;
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 25 -n $ins -t $i -c -r 10 -q 50
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 25 -n $ins -t $i -r 10 -q 50
done;
sleep 20;
for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 25 -n $ins -t $i -c -r 10 -q 90
done;
sleep 20;

for i in 8 16 32 64 128 256 512; do
    ins=$(( items / i ))
    ./kvp -a 2 -i 25 -n $ins -t $i -r 10 -q 90
done;
sleep 20;
