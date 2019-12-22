Delete code is in hashtable_delete_smart_lazy.c

basic idea of lazy delete is that when in lazy resizing (min subtable
is having its items copied up to larger subtables) will skip deleted
items and thus regain space. What this does is depending on the amount
of subtables/ratio of deletes to inserts it will determine if the new
subtable should have it size grown or just copy old size. Its
implemented in a way so that hopefully for
insert->delete->insert->delete... the subtable size will reach optimal
then just continue (this logic is in insertTable at the adddrop part).

The subtable array is now implemented as a circular linked list.
