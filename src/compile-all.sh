#!/bin/bash

make getcacheparms
Hashfunction=hash.o
make ../lib/${Hashfunction}
for table in hashtable hashtable_cache hashtable_lazy hashtable_lazy_cache hashtable_locks hashtable_cuckoo; do
    Hashtable=${table}.o
    echo "-- making $Hashtable"
    make ../lib/${Hashtable}
done
