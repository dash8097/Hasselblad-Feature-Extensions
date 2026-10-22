/* 生产常驻工人：复制完成后覆写/归还源图，累计继续使用独立快照。 */
#include "raw_stack_async.h"
#include "raw_stack_memory_budget.h"
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void pause_worker(void){Sleep(1);}
#else
#include <unistd.h>
static void pause_worker(void){usleep(1000);}
#endif
static unsigned permit,created,fail_at=5;
void rs_async_host_before_accumulate(unsigned index){
 (void)index;while(!__atomic_load_n(&permit,__ATOMIC_ACQUIRE))pause_worker();
}
int rs_async_host_thread_allowed(unsigned index){if(index==fail_at)return 0;created++;return 1;}
int main(void){
 assert(!rs_snapshot_budget(NULL,1));assert(!rs_snapshot_budget("MemAvailable: 100 kB\n",1));
 assert(rs_snapshot_budget("MemTotal: 400000 kB\nMemAvailable: 300000 kB\n",210510336));
 assert(!rs_snapshot_budget("MemAvailable: 200000 kB\n",210510336));
 assert(rs_snapshot_budget("MemAvailable: 65537 kB\n",1024));
 assert(!rs_snapshot_budget("MemAvailable: 65536 kB\n",1024));
 assert(!rs_snapshot_budget("MemAvailable: 18446744073709551616 kB\n",1024));
 assert(!rs_snapshot_budget("MemAvailable: 300000 MB\n",1024));
 for(int method=0;method<3;method++){
  RsConfig config={8,7,8,(RsMethod)method,0};size_t bytes=0;
  assert(rs_workspace_bytes(config,&bytes));void *workspace=malloc(bytes);
  RsCore core;assert(rs_init(&core,config,workspace,bytes));RsAsync a={0};uint16_t source[56],next[56];
  assert(!rs_async_start(&a,&core,source,sizeof source));created=0;
  assert(rs_async_prepare(&a,&core)&&created==4);struct RsAsyncPool *pool=a.pool;
  assert(rs_async_prepare(&a,&core)&&created==4);
  for(unsigned n=1;n<=100;n++){
   for(unsigned i=0;i<56;i++){source[i]=(uint16_t)(n*100);next[i]=(uint16_t)((n+1)*100);}
   __atomic_store_n(&permit,0,__ATOMIC_RELEASE);int ok=-1;
   assert(rs_async_start(&a,&core,source,sizeof source));
#ifdef RS_ASYNC_DIRECT
   for(unsigned wait=0;wait<5;wait++)pause_worker();
   RsAsyncTiming timing;assert(rs_async_timing(&a,&timing)&&!timing.copied&&!timing.finished);
#else
   while(!rs_async_source_read(&a))pause_worker();
   RsAsyncTiming timing;assert(rs_async_timing(&a,&timing)&&timing.copied&&!timing.finished);
#endif
   assert(rs_async_collect(&a,&ok)==0&&a.active&&core.frames==n-1);
   assert(!rs_async_shutdown(&a)&&!rs_async_start(&a,&core,next,sizeof next));
   /* 工人尚未累计，但已不借用源图；模拟原厂复用源缓存。 */
#ifndef RS_ASYNC_DIRECT
   memset(source,0xee,sizeof source);
#endif
   __atomic_store_n(&permit,1,__ATOMIC_RELEASE);
   while(rs_async_collect(&a,&ok)==0)pause_worker();
   memset(source,0xee,sizeof source);
   assert(rs_async_timing(&a,&timing)&&timing.copied&&timing.finished&&timing.total_ms>=timing.copy_ms);
   assert(ok&&core.frames==n&&!a.active&&a.source==NULL&&a.pool==pool&&created==4);
  }
  assert(rs_begin_finish(&core)&&rs_normalize_rows(&core,0,config.height));
  size_t out_bytes;const uint16_t *out=rs_compact(&core,&out_bytes);
  unsigned expected=method==RS_AVERAGE?5050:method==RS_MAXIMUM?10000:100;
  assert(out&&out_bytes==sizeof source);for(unsigned i=0;i<56;i++)assert(out[i]==expected);
  assert(rs_async_shutdown(&a)&&!a.pool&&rs_async_shutdown(&a));free(workspace);
 }
 for(fail_at=0;fail_at<4;fail_at++){
  _Alignas(8) uint32_t workspace[8];RsCore core;RsAsync a={0};assert(rs_init(&core,(RsConfig){2,4,2,RS_AVERAGE,0},workspace,sizeof workspace));
  created=0;assert(!rs_async_prepare(&a,&core)&&!a.pool&&created==fail_at);
  fail_at+=10;assert(rs_async_prepare(&a,&core));assert(rs_async_shutdown(&a));fail_at-=10;
 }
 /* 工人行范围跨越不连续累计段，仍只读各自的快照范围。 */
 for(unsigned height=1;height<=RS_MAX_SEGMENTS;height++){
  RsConfig config={5,height,8,RS_AVERAGE,0};RsCore core;RsAsync a={0};
  _Alignas(8) uint32_t storage[9][8];RsSegment segments[9];uint16_t source[72];
  for(unsigned i=0;i<height;i++)segments[i]=(RsSegment){storage[i],sizeof storage[i],1};
  assert(rs_init_segments(&core,config,segments,height));assert(rs_async_prepare(&a,&core));
  for(unsigned n=1;n<=3;n++){
   for(unsigned i=0;i<height*8;i++)source[i]=(uint16_t)(n*100+i);
   __atomic_store_n(&permit,1,__ATOMIC_RELEASE);assert(rs_async_start(&a,&core,source,height*16));
   int ok;while(rs_async_collect(&a,&ok)==0)pause_worker();assert(ok&&core.frames==n);
  }
  for(unsigned y=0;y<height;y++)for(unsigned x=0;x<8;x++)assert(storage[y][x]==600+3*(y*8+x));
  assert(rs_async_shutdown(&a));
 }
 puts("RAW_STACK_ASYNC_PERSISTENT_WORKERS_COPY_RELEASE_BEFORE_ACCUMULATION_AND_FINISH_PASSED");return 0;
}
