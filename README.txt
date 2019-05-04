non-blocking hashing idea:

dirs:

Data: has old data for memory usage approximations (no longer relevant).

debugging: has basic version that synchronizes threads before starting and a parsing program to read output and ensure its as expected (not relevant).

hash_prgs: has all the hashing programs: See individual files for comments. the non-block-ll/non-block-ll_trace is not included in the make file. In non-block-ll there is a commented out line starting with make: for build command. For comments look at hashint_trace.c, the rest are basically the same and didnt want to have to recopy basically exact same comments to to the rest of them.

	   - hash.c (outdated)- usage: ./prog <initial table size> <runs per thread> <num threads>
	   - hashint.c - usage: ./prog <initial table size> <runs per thread> <num threads> <num of rehash attempts>
	     	       This program will only insert. It inserts random unsigned longs into 
		       the table. The method of 'resizing' is x, 2x, 4x ...
	   - hashint2 - usage: ./prog <initial table size> <runs per thread> <num threads> <num of rehash attempts>
	     	      This program will only insert. It inserts random unsigned longs into 
		      the table. The method of 'resizing' is x, x, 2x, 2x, 4x... 
		      (for x > table_bound <- #defined constant).
	   - hashint_trace.c - usage: ./prog <initial table size> <num threads> <num hash attempts> <trace directory>
	     	      This program will insert and query. trace directories are in the spread_traces
		      directory. See spread_traces comments for more info.

	   - hashint_trace2.c - usage: ./prog <initial table size> <num threads> <num hash attempts> <trace directory>
	     	      Same as hashint_trace but does x,x, 2x.. method like hashint2.
		      		      
	   - non-block-ll.cpp -  usage: ./prog <num threads> <runs>
	     	      This is hash table using michael scott queue. Needs ALOT of work, has
		      race conditions still. Started with a copied version, on my todo list is 
		      rewrite it because this one is not great. Initial table size needs to be
		      defined in the program itself (under global variable initSize). Does 
		      not resize currently.

	   - non-block-ll_trace.cpp - usage: ./prog <num threads> <trace dir>
	              Same as non-block-ll.cpp but takes a trace dir same as hashint_trace
		      instead of <num runs>.

inputs: has txt files with alot of random ints/unsigned longs

spread_traces: has trace directors. each trace dir has a name like t_<type><number>.txt, i.e. t_ull1024.txt is a trace of 1024 unsigned longs. Each trace dir has 32 named p<num>.txt. The hashint_trace program assigns each thread its corresponding p<num>.txt file for running. The p<num>.txt files lines have an op then a number, three ops are 'A', 'T', and 'Q'. 'A' is to insert a number, 'T' and quick query for the number. The ratio of query to insert is about 6-1.

NOTE: you will need to make your own traces as github wont accept the files (im going to include samll ones to you can see format)
trace_programs: has c programs used to making input files, traces, and spread traces.
		- input.c - usage: ./prog <num items> <type ('l' for ull, anything else for int)>
		  	  Creates an input file named <type><num items>.txt
		- c_trace.c - usage: ./prog <input file>
		  	  Creates a trace file named t_<type><num items>.txt. Trace files have
			  the three operations from above
		- sp_tr.c  - usage: ./prog <trace file>
		  	   Spreads a trace file over 32 different files (duplicating some lines).
			   This program will make a new directory with the name of trace file 
			   inputted (without the extension). This is what hashint_trace takes as 
			   an argument for arg 4
