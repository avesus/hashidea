non-blocking hashing idea:

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
