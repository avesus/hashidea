# non-blocking hashing idea

## dir structure

bib/
- Relevant papers on the subject

lib/
- Has the hashtables source code

  hash.c: hash function being used, currently murmur32

  hashtable.h: api for using any of the hashtables below.

  index:
	_lazy  = copies elements from smallest table to large tables
	_local = instead of multiple seeds to get different slots for opentable hashing, looks
	         in buckets nearby the hash of a single seed.
	_ll    = uses linked list (or que) for buckets instead of opentable hashing.

		 
  hashtable.c: Opentable hashing, uses multiple seeds with hash function to generate
  	       different buckets, does not move items from smaller tables to large tables.

  hashtable_lazy.c: Opentable hashing, uses multiple seeds with hash function to generate
  		    different buckets, moves items from smaller table to large tables.

  hashtable_local.c: Opentable hashing, uses a single seed and checks nearby buckets to
  		     result of hash, does not move items from smaller tables to large tables.

  hashtable_lazy_local.c: Opentable hashing, uses a single seed and checks nearby buckets to
  			  result of hash, moves items from smaller table to large tables.

  hashtable_ll_tr.c: Linked list per bucket, uses single seed to determine bucket, resizes
  		     when table reaches a certain load factor, does not copy items from
		     smaller tables to large tables.

src/
- Contains wrapper code for testing the hash tables

  harness.c: contains main, to use ./harness -h and decide on appropriate args
  
  *.h: helper functions for parsing args, timing, etc..

## example method to run some tests, generate some data, and then visualize it

This is an example set of commands to generate data (including the
perfstat data) and then process it so it can be visualized for all the
different tables.  Assume user is in this directory at start.

```
cd src
./test_cache.sh > logname	# edit various declerations near top to control runs
				# if you want to just make sure things are working edit if around line 31
				# include name of previous log file if you had previously started a run and don't want to rerun previous tests
cd ???
???
```
