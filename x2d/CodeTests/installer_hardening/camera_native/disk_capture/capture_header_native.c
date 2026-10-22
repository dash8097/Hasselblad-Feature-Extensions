#include "capture_header_native.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
static int allowed(void *context){
 ChNative *n=context;CmNative *o=n?n->owner:NULL;
 return n&&n->active&&o&&o->api.base&&o->storage_owner&&
  o->api.qobject_thread(o->storage_owner)==o->api.current_thread();
}
static int read_memory(void *context,uintptr_t address,void *out,size_t bytes){
 ChNative *n=context;
 if(!allowed(n)||!out||!bytes||bytes>16384||address<4096||
    address>INT64_MAX||bytes>(uint64_t)INT64_MAX-address)return 0;
 size_t done=0;
 while(done<bytes){
  ssize_t count=pread(n->fd,(uint8_t*)out+done,bytes-done,(off_t)(address+done));
  if(count<0&&errno==EINTR)continue;
  if(count<=0||(size_t)count>bytes-done)return 0;
  done+=(size_t)count;
 }
 return 1;
}
int ch_native_open(ChNative *n,CmNative *owner,ChReadApi *out){
 if(!n||n->active||!owner||!out||!owner->api.base||!owner->storage_owner||
    owner->api.qobject_thread(owner->storage_owner)!=owner->api.current_thread())return 0;
 int fd=open("/proc/self/mem",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;
 *n=(ChNative){owner,fd,1};*out=(ChReadApi){read_memory,allowed,n};return 1;
}
int ch_native_close(ChNative *n){
 if(!allowed(n))return 0;
 int result=close(n->fd)==0;n->active=0;n->fd=-1;n->owner=NULL;return result;
}
