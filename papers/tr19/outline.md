Motivation:
Concurrency as means of speedup -> minimizing synchronization overhead

Implementation:
general - wait free/chaining subtables (CAS new subtable)
insert/query - first NULL/open table
resizing - lazy resizing vs thread (went with lazy resizing), allows
drop min subtable while maintaing first null
delete - how to while maintaining first NULL, implement as part of
lazy resizing (i.e don't NULL), delete while resizing need to add
bytes to the struct, making subtable array circular, delete/insert
ratio for determining next table size
cache - tags/line locality -> hash location + rest of cache line/line aligned 

Analysis:
baseline: O(n*logn)
resizing: O(c*n) -> needs works

Experimentation:
Tables to discuss(c) == cache - baseline(c), lazy resizing(c),
delete(c), locks, cuckoo
cache vs no cache performance / cache statistics
data set - random ints / memcached data set -> exploiting tags / cache
optimizations

Related:
cuckoo hashing

lock free -
doing all ops with CAS -> all entries must be done as pointers (64 bit machines)
maintaining a list is multiple ops per insert thus favor open table
first NULL principal, any two threads referencing same data must have
found one another at same first NULL or if one completed first will
have informed other
resizing normally requires locks to coordinate entries between 2+ tables. Race conditions arise in process for entry be doing added to T_1 and T_2. By chaining together we avoid that (all entries will continue via same path until added). Since num subtables can grow to n no issue of having to move entries quickly / in work inefficient method. No locking as all entries are capable of adding next subtable (CAS to determine if valid).
copying over elements from smaller subtables -> lazy vs thread (lazy because no rush). Work is distributed. 










      