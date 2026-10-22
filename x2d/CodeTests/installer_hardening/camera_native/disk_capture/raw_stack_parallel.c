#include "raw_stack_parallel.h"
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
typedef HANDLE RsThread;
#define ENTRY DWORD WINAPI
#define DONE 0
#else
#include <pthread.h>
typedef pthread_t RsThread;
#define ENTRY void *
#define DONE NULL
#endif
typedef struct {RsCore *core;const uint16_t *source;uint32_t first,rows;int finish,ok;} Job;
static ENTRY work(void *context){
 Job *j=context;j->ok=0;
 if(j->finish){j->ok=rs_normalize_rows(j->core,j->first,j->rows);return DONE;}
 uint32_t stride=j->core->config.stride_samples,block_rows=j->rows<64?j->rows:64;
 size_t cap=(size_t)block_rows*stride*2;uint16_t *cache=malloc(cap);
 if(!cache)return DONE;
 j->ok=1;
 for(uint32_t y=j->first;y<j->first+j->rows;y+=block_rows){
  uint32_t n=j->first+j->rows-y;if(n>block_rows)n=block_rows;
  size_t bytes=(size_t)n*stride*2;
  memcpy(cache,j->source+(size_t)y*stride,bytes);
  if(!rs_accumulate_block(j->core,cache,bytes,y,n)){j->ok=0;break;}
 }
 free(cache);return DONE;
}
static int run(RsCore *s,const uint16_t *source,unsigned count,int finish){
 if(!count||count>4||!s)return 0;
 if(count>s->config.height)count=s->config.height;
 RsThread thread[4];Job job[4];unsigned created=0;int ok=1;
 for(unsigned i=0;i<count;i++){
  uint32_t first=(uint64_t)s->config.height*i/count,end=(uint64_t)s->config.height*(i+1)/count;
  job[i]=(Job){s,source,first,end-first,finish,0};
#ifdef _WIN32
  thread[i]=CreateThread(NULL,0,work,job+i,0,NULL);
  if(!thread[i]){ok=0;break;}
#else
  if(pthread_create(thread+i,NULL,work,job+i)){ok=0;break;}
#endif
  created++;
 }
 for(unsigned i=0;i<created;i++){
  /* 不能在未知存活工人仍借用源 RAW/栈上 Job 时返回给所有者释放。 */
#ifdef _WIN32
  if(WaitForSingleObject(thread[i],INFINITE)!=WAIT_OBJECT_0)abort();
  CloseHandle(thread[i]);
#else
  if(pthread_join(thread[i],NULL))abort();
#endif
  if(!job[i].ok)ok=0;
 }
 if(!ok)s->failed=1;
 return ok;
}
int rs_parallel_frame(RsCore *s,const uint16_t *source,size_t bytes,unsigned threads){
 if(!source||!rs_begin_frame(s,bytes))return 0;
 return run(s,source,threads,0)&&rs_commit_frame(s);
}
const uint16_t *rs_parallel_finish(RsCore *s,unsigned threads,size_t *bytes){
 if(!s||s->segment_count)return NULL;
 if(!rs_begin_finish(s)||!run(s,NULL,threads,1))return NULL;
 return rs_compact(s,bytes);
}
unsigned rs_parallel_finish_blocks(RsCore *s,unsigned threads,RsPixelBlock *blocks,unsigned capacity,size_t *bytes){
 if(!s||!blocks||capacity<(s->segment_count?s->segment_count:1)||!bytes||
    !rs_begin_finish(s)||!run(s,NULL,threads,1))return 0;
 return rs_compact_blocks(s,blocks,capacity,bytes);
}
