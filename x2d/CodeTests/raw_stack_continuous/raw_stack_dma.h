/* 官方 4.2.0 HAL：自有缓存快照；源租约由调用方持有到完成。
 * 实机验证单次16MiB，整帧分13块；不将未知完成状态当作可释放。 */
#ifndef RAW_STACK_DMA_H
#define RAW_STACK_DMA_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
 int (*allocate)(void*,void**,uint32_t,uint32_t,uint32_t,uint32_t);
 int (*map)(void*,void**),(*unmap)(void*),(*free_mem)(void*),(*size)(void*,uint32_t*),(*sync)(void*,uint32_t);
 int (*request)(void*,uint32_t,void**),(*submit)(void*,void*,void*,uint64_t,void*,uint64_t,uint64_t,void(*)(void*),void*);
 int (*issue)(void*,void*),(*terminate)(void*,void*),(*release)(void*,void*);
 uint64_t (*clock_ms)(void);void (*pause_ms)(void);
} RsDmaApi;
typedef struct {RsDmaApi api;void *device,*memory,*mapping,*channel;size_t capacity;int complete,uncertain;} RsDma;
static void rs_dma_complete(void *p){__atomic_store_n(&((RsDma*)p)->complete,1,__ATOMIC_RELEASE);}
static int rs_dma_close(RsDma *d){
 if(d->uncertain)return 0;
 if(d->channel){if(d->api.release(d->device,d->channel))return 0;d->channel=NULL;}
 if(d->mapping){if(d->api.unmap(d->memory))return 0;d->mapping=NULL;}
 if(d->memory){if(d->api.free_mem(d->memory))return 0;d->memory=NULL;}
 d->capacity=0;return 1;
}
static int rs_dma_prepare(RsDma *d,RsDmaApi a,void *device,size_t bytes){
 if(!d||d->memory||d->channel||!device||!bytes||bytes>UINT32_MAX||!a.allocate||!a.map||!a.unmap||!a.free_mem||!a.size||!a.sync||!a.request||!a.submit||!a.issue||!a.terminate||!a.release||!a.clock_ms||!a.pause_ms)return 0;
 d->api=a;d->device=device;uint32_t actual=0;
 if(a.allocate(device,&d->memory,(uint32_t)bytes,1024,2,0x10)||!d->memory||a.size(d->memory,&actual)||actual<bytes||a.map(d->memory,&d->mapping)||!d->mapping||a.request(device,0,&d->channel)||!d->channel)return 0;
 d->capacity=bytes;return 1;
}
static int rs_dma_copy(RsDma *d,void *source,uint64_t source_offset,size_t bytes){
 uint32_t source_size=0;
 if(!d||d->uncertain||!d->channel||!d->mapping||!source||!bytes||bytes>d->capacity||d->api.size(source,&source_size)||source_offset>source_size||bytes>source_size-source_offset||((source_offset|bytes)&31))return 0;
 /* 清理目标CPU缓存后交给DMA；完成后invalidate，再给累计工人。 */
 if(d->api.sync(d->memory,2))return 0;
 for(size_t offset=0;offset<bytes;){
  size_t n=bytes-offset;if(n>16*1024*1024)n=16*1024*1024;
  __atomic_store_n(&d->complete,0,__ATOMIC_RELEASE);d->uncertain=1;
  int rc=d->api.submit(d->device,d->channel,d->memory,offset,source,source_offset+offset,n,rs_dma_complete,d);
  if(!rc)rc=d->api.issue(d->device,d->channel);
  uint64_t start=d->api.clock_ms();
  while(!rc&&!__atomic_load_n(&d->complete,__ATOMIC_ACQUIRE)&&d->api.clock_ms()-start<3000)d->api.pause_ms();
  if(rc||!__atomic_load_n(&d->complete,__ATOMIC_ACQUIRE)){
   if(!d->api.terminate(d->device,d->channel))d->uncertain=0;
   return 0;
  }
  d->uncertain=0;offset+=n;
 }
 return d->api.sync(d->memory,1)==0;
}
#endif
