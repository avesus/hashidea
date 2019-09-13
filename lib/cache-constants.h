#include <stdio.h>

static int 
getCacheLineSize(void)
{
  FILE* fp=fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
  if(!fp){
    return 0;
  }
  char buf[16]="";
  if(!fgets(buf, 8, fp)){
    return 0;
  }
  return atoi(buf);
}

static const int lineSize = L1_Cache_Line_Size;
static const int logLineSize = L1_Log_Cache_Line_size;
static const int entPerLine = L1_Cache_Line_Size/sizeof(entry);

////////////////////////////////////////////////////////////////
// return 0 if everything is ok.
// return nonzero if there is a problem: 
// 	1 -> wrong cache line size, 
// 	2 -> entries aren't multiple of cacheline 
int
checkCompiledCorrectly(void)
{
  if (lineSize != getCacheLineSize()) return 1;
  if (sizeof(entry)*entPerLine != lineSize) return 2;
  return 0;
}
