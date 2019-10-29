#!/bin/bash

for i in 128 256 512 1024 2048; do
    echo ./tm $i
    sudo perf stat -d -d -d -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses,L1-dcache-prefetches,L1-dcache-prefetch-misses,L1-icache-loads,L1-icache-load-misses,L1-icache-prefetches,L1-icache-prefetch-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,LLC-prefetches,LLC-prefetch-misses,dTLB-loads,dTLB-load-misses,dTLB-stores,dTLB-store-misses,dTLB-prefetches,dTLB-prefetch-misses,iTLB-loads,iTLB-load-misses,branch-loads,branch-load-misses,node-loads,node-load-misses,node-stores,node-store-misses,node-prefetches,node-prefetch-misses ./tm $i
    done
