#ifndef RAW_STACK_MEMORY_BUDGET_H
#define RAW_STACK_MEMORY_BUDGET_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* 只读 Linux 容量字段；此预检不代表后续系统没有其他内存竞争。 */
static inline int rs_snapshot_budget(const char *text,size_t snapshot_bytes){
 if(!text)return 0;
 const char *p=text;
 while(*p){
  if(!strncmp(p,"MemAvailable:",13)){
   p+=13;while(*p==' '||*p=='\t')p++;
   if(*p<'0'||*p>'9')return 0;uint64_t kb=0;
   do{unsigned digit=(unsigned)(*p++-'0');if(kb>(UINT64_MAX-digit)/10)return 0;kb=kb*10+digit;}while(*p>='0'&&*p<='9');
   while(*p==' '||*p=='\t')p++;
   if(strncmp(p,"kB\n",3)||kb>UINT64_MAX/1024)return 0;
   uint64_t reserve=UINT64_C(64)*1024*1024;
   return snapshot_bytes<=UINT64_MAX-reserve&&kb*1024>=(uint64_t)snapshot_bytes+reserve;
  }
  p=strchr(p,'\n');if(!p)return 0;p++;
 }
 return 0;
}
#endif
