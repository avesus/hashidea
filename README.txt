non-blocking hashing idea:

all programs have following usage: ./prog <initial table size> <runs per threads> <num threads (max 32)>

hash.c does hashing using strings as a key

hashint.c does hashing using unsigned longs/ints as a key

testscript.py will run debug_hashint.c. Basically just hashint.c but synchronizes threads at a barrier and will output results output.txt which parse will check to see if there are any duplicates. debug_hashint.c will also print the full table if the amount of items in the table is not whats expected (this can hyptothetically happen without a bug if it doesnt hit all ints in rand()%test_num).

testtime.py is a coarse timing program, arg 1 is amount of loops (10 tests per loop). To specify what you want to test (i.e init size, runs per thread, and num_threads) you have to modify the program, right now just mods i by array lengths with some predefined constants. arg 2 is a bool, 1 if you want to test hashint.c and lhi.c (output to intnb.txt and intlock.txt respectively), 0 for hash.c and lh.c (output to strnb.txt and strlock.txt respectively). Right now just pipes linux builtin time function, really just giving me an idea. Since the locking hash table does not resize I always ensure it has size>=total calls (num_threads*runs) just so i'm not cheating in my favor. 

lh.c is locking hash table with strings as a key

lhi.c is locking hash table with unsigned longs/ints as a key

words.txt has 5mb of unique words for testing hash.c/lh.c
