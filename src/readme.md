# creating executable

default makefile will create harness using ../lib/hashtable.o.  If you want a different hashtable, e.g., lazy one, then

'''
/bin/rm harness
make Hashtable=hashtable_lazy.o
'''

If you want a different hashfunction, you can specify it with
`Hashfunction=x.o`, where `../lib/x.o` is the object file that
contains the hash function.

Hashtable.c is baseline hashtable
Postfixes:
_lazy: Means that inserting threads will reinsert entries from the smallest table and update the index for the starting table.
_delete: has delete functionality
_cache: means code has been written with cache efficiency in mind

Special cases:
hashtable_delete_thr: Means there is an additional thread that handle resizing the table/clearing deleted entries.

Delete Summary:

Entry struct has been modified to include another unsigned long which
is used a boolean to tell whether the entry has been deleted or
not. This is necessary for when items are deleted while being operated
on in another way (i.e delete call to an item being moved up from a
small table to a larger one).

Delete_thr: 

Basically each subtable has an int which stores approximately how many
delete calls there have been to that table, if the amount of delete
calls is above some threshhold (and the delete thread is not currently
in session) the deleter thread will be signalled (basically a mutex
its sleeping at will be unlocked). The delete thread will go through
all entries in the subtable and reinsert them (only to larger tables
i.e the insertion will start at the index of the subtable being
deleted +1). If current start index subtable has had all its elements
moved, the start index will be incremented. While deletion is taking
place it is possible for there to be duplicate entries though freeall
(end of a trial) waits until the delete thread has finished whatever
table it is working on. The delete thread sets the 1s bit in the entry
ptr for NULL and deleted items. After this those subtable
locations/entries will no longer be able to be undeleted/inserted
into.

Delete_lazy: 

Normal lazy resizing but items who have been deleted are not
reinserted. If an entry being inserted matches a deleted value, the
matching entry will NOT be undeleted if it is in the smallest subtable
(subtable currently having all its entries reinserted).

	
Cache Variables:

As constats we want: cache line size, log_2(cache line size), entries per line, log_2(entries per line)
- lineSize: cache line size in bytes
- entPerLine: entries per line
- logLineSize: log_2(cache line size)

cache constants for a particilar machine are generated with
`getcacheparms`.  In the library code, e.g., `hashtable_cache.c` you
get access to the cache constants include the file
`cache-constants.h`.  It includes the definition of the constants and
exposes `checkCompiledCorrectly` which should be called from the
program that uses the hash library.  if it returns 0, everything is
ok.
