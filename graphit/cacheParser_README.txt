program takes a file, the table to parse, and the relevant paramaters (in that order)
i.e
os.systems(python3 cacheParser.py <file> <table> <params...>)
params is an optional argument
say I wanted to get the cache stats for hashtable_cache_np with inserts == 16000000, qp == 0, t==1, i==24, lines==1, etc...
i would run
python3 cacheParse.py ../src/2019-10-30T19:33:12-04:00.test.log hashtable_cache_np --inserts 16000000 --qp 0 -t 1 -i 24 -a 2 --lines 1 --args --ci 1 --sc 4 --ec 4

Right now it only outputs in a human readable format. We need it to be csv, what you need to do is insert into getstats.py where cacheParser.py needs to be called and what it should output at each point (i.e csv header should probably have total misses/hits/miss rate appened to it and then at certain point cacheparser should be called to print those out and have those appended to the line). just add comments for where that should be, the arguments it should take, and what the printout should be. i.e if you needed hits to be printed somewhere you would comment:
#cache parser call here, need hits to be printed, the file is in variable X, the table is in variable Y, the params are in variable(s) A,B,C.
please also include comments/or the commented out code for what I should do with the output i.e
#append the result of cache parser with ... to variable O
if you setup an empty string and do the proper ops on it that would be best imo i.e
getstats.py would look like:
cacheParserResult = ""
#do stuff with cacheParser.py and have it go to cacheParserResult
outfile.write(cacheParserResult)
that would likeliest be the easiest way to do all this.
