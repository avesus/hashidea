# creating executable

default makefile will create harness using ../lib/hashtable.o.  If you want a different hashtable, e.g., lazy one, then

'''
/bin/rm harness
make Hashtable=hashtable_lazy.o
'''

If you want a different hashfunction, you can specify it with
`Hashfunction=x.o`, where `../lib/x.o` is the object file that
contains the hash function.

