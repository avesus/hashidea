https://en.wikipedia.org/wiki/Cache_prefetching

Software prefetching:
Not happening (we cannot see any prefetch calls in dissasembled code).


Hardware prefetching:

https://mirrors.edge.kernel.org/pub/linux/utils/cpu/msr-tools/
tool for disabling/enabling hardware prefetching/checking if its enabled are not

https://stackoverflow.com/questions/784041/how-do-i-programmatically-disable-hardware-prefetching
hardware prefetch status is in "/dev/cpu/<cpu num>/msr"

Can check status with "./rdmsr 0x1a4"
Status 0 indicates all prefetchers are on

https://software.intel.com/en-us/articles/disclosure-of-hw-prefetcher-control-on-some-intel-processors
4 bits indicate prefetching status. 2 prefetchers for each L1 and L2 cache, a 1s bit in any of the 4 slots indicates that prefetcher is disabled.

https://stackoverflow.com/questions/10145857/unable-to-disable-the-hardware-prefetcher
gives instructions for how exactly to disable/enable prefetchers for a given cpu

http://man7.org/linux/man-pages/man4/msr.4.html
sudo modprobe msr enables the driver to allow reading/writing to MSR registers which will determine which events are caught by hardware counters

Perfstat is using hardware counters:
https://perf.wiki.kernel.org/index.php/Main_Paeg
Can be seen that skylake (our architecture) is doing so at "/linux/tools/perf/pmu-events/arch/x86/skylake"

Rundown of prefetching:
https://gcc.gnu.org/projects/prefetch.html#ia64

sudo perf list --long-desc gives better picture of what going on (but still lacking)

It may be the case that perf stat has the wrong hardware event!
https://stackoverflow.com/questions/42098292/measure-the-number-of-lines-loaded-in-l1-l2-cache-for-readsincluding-prefetch


Hardware codes descriptions for skylake:
https://download.01.org/perfmon/index/skylake.html
in skylake directory are the actual structs


Possibly useful tool for using the perf tool:
https://github.com/andikleen/pmu-tools

A reasonable guide:
https://www.halobates.de/modern-pmus-yokohama.pdf

likwid library
