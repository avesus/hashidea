non-blocking hashing idea:

hash.c does hashing using strings as a key

hashint.c does hashing using unsigned longs/ints as a key

testscript.py will run debug_hashint.c. Basically just hashint.c but synchronizes threads at a barrier and will output results output.txt which parse will check to see if there are any duplicates. debug_hashint.c will also print the full table if the amount of items in the table is not whats expected (this can hyptothetically happen without a bug if it doesnt hit all ints in rand()%test_num).

lh.c is locking hash table with strings as a key

lhi.c is locking hash table with unsigned longs/ints as a key

words.txt has 5mb of unique words for testing hash.c/lh.c
