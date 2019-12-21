package main



//#include <stdlib.h>
import "C"

import (
	"fmt"
	"unsafe"
	"math/rand"
	"time"
	"sync/atomic"
	"sync"
	"github.com/pborman/getopt"
	"runtime/debug"
)
//my int64 for casting to byte array for hash
type hint64 int64;

//for using cmalloc to create slices
type caster struct {
	ptr *byte;
	len int64;
	cap int64;
}

//for timing
type myBarrier struct {
	barrier sync.WaitGroup;
	timer time.Time;
}

type entry struct {
	val hint64;
}
type subtable struct{
	table []*entry;
}
type hashtable struct {
	tables []*subtable;
	seeds []uint32;
	tnum int32;
	cmode int8;
}


type align64 struct {
	e []entry;
	q []int;
	padding [2]int64;
}


var startBarrier myBarrier
var endBarrier myBarrier


//wg for sync
var wg sync.WaitGroup



//args
var oHelp *bool = new(bool); 
var verbose *int = new(int);
var check *bool = new(bool);
var cstyle *int8 = new(int8);
var att *int = new(int);
var maxST *int32 = new(int32);
var initSize *uint = new(uint);
var trials *int = new(int);
var nthreads *int = new(int);
var inserts *int = new(int);
var queryPercent *int = new(int);


//consts for lookup return vals
const in int = -1
const unk int = -2
const notIn int = -3

func cnew(type_size uintptr) unsafe.Pointer {
	temp := (*byte)(C.calloc(1,C.ulong(type_size)));
	return unsafe.Pointer(&temp)
}
func cmake_slice(size uint32, type_size uintptr) unsafe.Pointer {
	var temp caster;
	temp.ptr=(*byte)(C.calloc(C.ulong(size),C.ulong(type_size)));
	temp.len=int64(size);
	temp.cap=int64(size);
	return unsafe.Pointer(&temp);

}



func createSubTable(cmode int8, size uint32) *subtable {
	var newTable *subtable;
	if cmode > 0  {
		if cmode > 1 {
			newTable=*(**subtable)(cnew(unsafe.Sizeof(*newTable)));
		}else {
			newTable = new(subtable);
		}
		newTable.table=*(*[]*entry)(cmake_slice(size, unsafe.Sizeof(newTable.table[0])));
	} else {
		newTable = new(subtable);
		newTable.table = make([]*entry,  size);
	}
	return newTable;
}
func initTable(attempts int, isize uint, nthread int, seed []uint32, cmode int8) *hashtable {
	var newTable *hashtable
	if cmode > 1 {
		newTable=*(**hashtable)(cnew(unsafe.Sizeof(*newTable)));

	}else {
		newTable = new(hashtable);
	}

	newTable.seeds=seed;
	newTable.tnum = 1;
	newTable.cmode = cmode;
	if cmode > 1 {
		newTable.tables=*(*[]*subtable)(cmake_slice(uint32(*maxST), unsafe.Sizeof(newTable.tables[0])));
	} else {
		newTable.tables = make([]*subtable,  *maxST);
	}
	newTable.tables[0] = createSubTable(cmode, 1<<isize);
	return newTable;
}


func (val hint64) hash(seed uint32) uint32 {
	var h uint32= seed;
	var key uint64 = uint64(val);
	for i:=uint32(0);i<2;i++ {
		var k uint32 = uint32(key>>(i<<5));
		k *= 0xcc9e2d51;
		k = (k << 15) | (k >> 17);
		k *= 0x1b873593;
		h ^= k;
		h = (h << 13) | (h >> 19);
		h = h * 5 + 0xe6546b64;
	}
	h ^= 8;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

func (tcur subtable) lookup(ent *entry, slot uint32) int{
	if tcur.table[slot] == nil {
		return int(slot);
	}else if tcur.table[slot].val == ent.val {
		return in
	}
	return unk;
}


func (tcur subtable) lookupQuery(val hint64, slot uint32) int{
	if tcur.table[slot] == nil {
		return notIn;
	}else if tcur.table[slot].val == val {
		return int(slot);
	}
	return unk;
}

func (ht* hashtable) addSubTable(newTable *subtable, tslot int32) {
	var expec *subtable = nil;
	var res bool = atomic.CompareAndSwapPointer(
		(*unsafe.Pointer)(unsafe.Pointer(&ht.tables[tslot])),
		unsafe.Pointer(expec),
		unsafe.Pointer(newTable));
	if res == true {
		var newSize int32 = tslot + 1;
		atomic.CompareAndSwapInt32(&ht.tnum, tslot, newSize);
	}else {
		if ht.cmode > 0 {
			C.free(unsafe.Pointer(&newTable.table[0]));
			if ht.cmode > 1 {
				C.free(unsafe.Pointer(newTable));
			}
		}
		var newSize int32 = tslot + 1;
		atomic.CompareAndSwapInt32(&ht.tnum, tslot, newSize);
	}
	
}


func (ht* hashtable) query(val hint64) *entry{

	var buckets []uint32 = make([]uint32, len(ht.seeds));
	for i:=0;i<len(ht.seeds);i++ {
		buckets[i]=val.hash(ht.seeds[i]);
	}
	var tcur *subtable;
	for i:=int32(0);i<ht.tnum;i++ {
		tcur = ht.tables[i];
		for j:=0;j<len(ht.seeds);j++ {
			var res int = tcur.lookupQuery(val, buckets[j]&(uint32(len(tcur.table)-1)));
			if res == unk {
				continue;
			}else if res == notIn {
				return nil;
			}else {
				return tcur.table[res];
			}
		}
		
	}
	return nil;
}


func (ht* hashtable) insert(ent *entry) int{

	var buckets []uint32 = make([]uint32, len(ht.seeds));
	for i:=0;i<len(ht.seeds);i++ {
		buckets[i]=ent.val.hash(ht.seeds[i]);
	}
	var lcur int32 = ht.tnum;
	var tcur *subtable;
	for {

		for i:=int32(0);i<lcur;i++ {
			tcur = ht.tables[i];
			for j:=0;j<len(ht.seeds);j++ {
				var res int = tcur.lookup(ent, buckets[j]&(uint32(len(tcur.table)-1)));
				if res == unk {
					continue;
				}else if res == in {
					return 0;
				}else {
					var expec *entry = nil;
					var cmp bool = atomic.CompareAndSwapPointer(
						(*unsafe.Pointer)(unsafe.Pointer(&tcur.table[res])),
						unsafe.Pointer(expec),
						unsafe.Pointer(ent));
					if cmp == true {
						return 1;
					}else {
						if tcur.table[res].val == ent.val {
							return 0;
						}
					}
				}
			}
			lcur = ht.tnum;
		}
		var newTable *subtable = createSubTable(ht.cmode, uint32(len(tcur.table)<<1));
		ht.addSubTable(newTable, lcur);
	}
}

func (ht* hashtable) inserter(tid int, ninserts int, e []align64) {
	if ht.cmode > 0 {
		e[tid].e=*(*[]entry)(cmake_slice(uint32(ninserts), unsafe.Sizeof(e[tid].e[0])));
		e[tid].q=*(*[]int)(cmake_slice(uint32(ninserts), unsafe.Sizeof(e[tid].q[0])));
	} else {
		e[tid].e = make([]entry, ninserts);
		e[tid].q = make([]int, ninserts);
	}


	for i:=0;i<ninserts;i++ {
		e[tid].q[i] = rand.Int()%100;
		e[tid].e[i].val=hint64(rand.Int63());
	}

	startBarrier.barrier.Done();
	startBarrier.barrier.Wait();
	if tid == 0 {
		startBarrier.timer = time.Now();
	}
	for i:=0;i<ninserts;i++ {
		if e[tid].q[i] < *queryPercent {
			ht.query(e[tid].e[i].val);
		}else {
			ht.insert(&e[tid].e[i]);
		}
	}
	
	endBarrier.barrier.Done();
}

func (ht hashtable) results() {
	var counter []int = make([]int, ht.tnum);
	var corrTester map[int64]bool = make(map[int64]bool);
	for i:=int32(0);i<ht.tnum;i++ {
		for j:=0;j<len(ht.tables[i].table);j++ {
			if ht.tables[i].table[j] != nil {
				if *check {
					corrTester[int64(ht.tables[i].table[j].val)] = true;
				}
				counter[i]++;
			}
		}
		if ht.cmode > 0 && *check == false {
			C.free(unsafe.Pointer(&ht.tables[i].table[0]));
			if ht.cmode > 1 {
				C.free(unsafe.Pointer(ht.tables[i]));
			}
		}
	}
	if *verbose > 0 {
		var totalItems int = 0;
		var totalSpace uint32 = 0;
		for i:=int32(0);i<ht.tnum;i++ {
			totalItems+= counter[i];
			totalSpace+=uint32(len(ht.tables[i].table));
			if *verbose > 1 {
				fmt.Printf("%d: %d/%d\n",i, counter[i], len(ht.tables[i].table));
			}
		}
		if *check {
			if totalItems != len(corrTester) {
				fmt.Printf("Double Added: %d vs %d\n", totalItems, len(corrTester));
			} else {
				if *verbose > 0 {
					fmt.Printf("Check Item Amount: %d == %d\n", totalItems, len(corrTester));
				}
			}

		}
		fmt.Printf("Total: %d/%d\nRuntime: %s\n", totalItems, totalSpace, endBarrier.timer.Sub(startBarrier.timer));
		
	}
}
func main()  {

	//parse arguments
	verbose = getopt.IntLong("verbose", 'v', 0, "verbose mode");
	check = getopt.BoolLong("check", 'o', "correctness check");
	cstyle = (*int8)(unsafe.Pointer(getopt.Int32Long("cstyle", 'c', 0, "use c style allocation")));
	nthreads = getopt.IntLong("threads", 't', 1, "num threads");
	maxST = getopt.Int32Long("max", 'm', 32, "max subtables");
	inserts = getopt.IntLong("inserts", 'n', 0, "inserts per thread");
	queryPercent = getopt.IntLong("qp", 'q', 0, "query percentage (int 0-100)");
	att = getopt.IntLong("attempts", 'a', 1, "hash attempts");
	initSize = getopt.UintLong("",'i', 0, "init size (give as power of 2)");
	trials = getopt.IntLong("trials",'r', 1, "trials to run");
	oHelp = getopt.BoolLong("help", 'h', "display usage");
	getopt.Parse();
	if *oHelp == true {
		getopt.Usage()
		return;
	}
	if *cstyle > 2 {
		debug.SetGCPercent(-1)
	}
	//initializing variables
	var seeds []uint32;
	if *cstyle > 1 {
		seeds=*(*[]uint32)(cmake_slice(uint32(*att), unsafe.Sizeof(seeds[0])));

	}else {
		seeds = make([]uint32, *att);
	}

	rand.Seed(time.Now().UTC().UnixNano());
	for i:=0;i<*att;i++ {
		seeds[i] = uint32(rand.Int());
	}
	var startTimes []time.Time = make([]time.Time, *trials);
	var endTimes []time.Time = make([]time.Time, *trials);
	for t:=0;t<*trials;t++ {
		var ht *hashtable = initTable(*att, *initSize,*nthreads,seeds, *cstyle);

		//start testing
		startBarrier.barrier.Add(*nthreads);
		endBarrier.barrier.Add(*nthreads);
		var e []align64;
		if *cstyle > 2 {
			var temp caster;
			temp.ptr=(*byte)(C.aligned_alloc(
				C.ulong(*nthreads)*C.ulong(unsafe.Sizeof(e[0])+64),
				C.ulong(unsafe.Sizeof(e[0]))));
			temp.len=int64(*nthreads);
			temp.cap=int64(*nthreads);
			e=*(*[]align64)(unsafe.Pointer(&temp));
		}else{
			e=make([]align64, *nthreads);
		}
		var isAligned *int64 = (*int64)((unsafe.Pointer(&e)));
		if (*isAligned)%64 != 0 {
			fmt.Printf("Error aligned input array\n... Exitting\n");
			return;
		}

		for i :=0; i<*nthreads;i++ {
			go ht.inserter(i, *inserts, e);
		}
		endBarrier.barrier.Wait();
		endBarrier.timer = time.Now();
		endTimes[t]=endBarrier.timer;
		startTimes[t]=startBarrier.timer;
		if ht.cmode > 0 && *check==false {
			for i :=0; i<*nthreads;i++ {
				C.free(unsafe.Pointer(&e[i].e[0]));
				C.free(unsafe.Pointer(&e[i].q[0]));
			}

		}
		if ht.cmode > 2 {
			C.free(unsafe.Pointer(&e[0]));
		}

		
		//test/print results
		ht.results();
		if ht.cmode > 1 {
			C.free(unsafe.Pointer(&ht.seeds[0]));
			C.free(unsafe.Pointer(&ht.tables[0]));
			C.free(unsafe.Pointer(ht));
		}
	}
	fmt.Printf("--------------------------------------------------------------\n");
	fmt.Printf("Results %d Runs\n", *trials);
	fmt.Printf("Params: Attempts = %d, InitSize = %d, Inserts = %d, QueryPercent = %d, Threads = %d, Cstyle = %d\n",
		*att, *initSize, *inserts,*queryPercent, *nthreads, *cstyle);
	for t:=0;t<*trials;t++ {
		fmt.Printf("Run[%d] -> %s\n", t, endTimes[t].Sub(startTimes[t]));
	}
	fmt.Printf("--------------------------------------------------------------\n");
	
}




