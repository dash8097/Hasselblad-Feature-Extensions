#include "raw_stack_async.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static uint64_t monotonic_ns(void){LARGE_INTEGER n,f;return QueryPerformanceCounter(&n)&&QueryPerformanceFrequency(&f)?(uint64_t)(n.QuadPart/f.QuadPart)*1000000000+(uint64_t)(n.QuadPart%f.QuadPart)*1000000000/f.QuadPart:0;}
typedef HANDLE Thread;
typedef CRITICAL_SECTION Mutex;
typedef CONDITION_VARIABLE Condition;
#define ENTRY DWORD WINAPI
static void lock(Mutex *m){EnterCriticalSection(m);}
static void unlock(Mutex *m){LeaveCriticalSection(m);}
static void wait(Condition *c,Mutex *m){if(!SleepConditionVariableCS(c,m,INFINITE))abort();}
static void wake(Condition *c){WakeAllConditionVariable(c);}
#else
#include <pthread.h>
#include <time.h>
static uint64_t monotonic_ns(void){struct timespec t;return clock_gettime(CLOCK_MONOTONIC,&t)?0:(uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
typedef pthread_t Thread;
typedef pthread_mutex_t Mutex;
typedef pthread_cond_t Condition;
#define ENTRY void *
static void lock(Mutex *m){if(pthread_mutex_lock(m))abort();}
static void unlock(Mutex *m){if(pthread_mutex_unlock(m))abort();}
static void wait(Condition *c,Mutex *m){if(pthread_cond_wait(c,m))abort();}
static void wake(Condition *c){if(pthread_cond_broadcast(c))abort();}
#endif
typedef struct RsAsyncPool Pool;
typedef struct {Pool *pool;unsigned index;} Worker;
struct RsAsyncPool {
 Mutex mutex;Condition changed;Thread threads[4];Worker workers[4];
 unsigned created,count,stop,copied,finished;uint64_t epoch;
 uint32_t height,stride;size_t capacity;uint16_t *snapshot;RsAsync *task;
 int ok;
};
#ifdef RS_ASYNC_HOST_CHECK
extern void rs_async_host_before_accumulate(unsigned);
extern int rs_async_host_thread_allowed(unsigned);
#endif
static ENTRY work(void *context){
 Worker *worker=context;Pool *p=worker->pool;uint64_t seen=0;
 lock(&p->mutex);
 for(;;){
  while(!p->stop&&seen==p->epoch)wait(&p->changed,&p->mutex);
  if(p->stop)break;
  seen=p->epoch;RsAsync *a=p->task;
  uint32_t first=(uint64_t)p->height*worker->index/p->count;
  uint32_t end=(uint64_t)p->height*(worker->index+1)/p->count;
  const uint16_t *source=a->source;RsCore *core=a->core;
  size_t offset=(size_t)first*p->stride,bytes=(size_t)(end-first)*p->stride*2;
  unlock(&p->mutex);
  memcpy(p->snapshot+offset,source+offset,bytes);
  lock(&p->mutex);
  if(++p->copied==p->count){a->source_read_ns=monotonic_ns();__atomic_store_n(&a->source_read,1,__ATOMIC_RELEASE);}
  unlock(&p->mutex);
#ifdef RS_ASYNC_HOST_CHECK
  rs_async_host_before_accumulate(worker->index);
#endif
  int ok=rs_accumulate_block(core,p->snapshot+offset,bytes,first,end-first);
  lock(&p->mutex);
  if(!ok)p->ok=0;
  if(++p->finished==p->count){
   a->ok=p->ok&&rs_commit_frame(core);
   if(!a->ok)core->failed=1;
   a->done_ns=monotonic_ns();
   __atomic_store_n(&a->done,1,__ATOMIC_RELEASE);
  }
 }
 unlock(&p->mutex);return 0;
}
static void close_pool(Pool *p){
 lock(&p->mutex);p->stop=1;wake(&p->changed);unlock(&p->mutex);
 for(unsigned i=0;i<p->created;i++){
#ifdef _WIN32
  if(WaitForSingleObject(p->threads[i],INFINITE)!=WAIT_OBJECT_0)abort();
  CloseHandle(p->threads[i]);
#else
  if(pthread_join(p->threads[i],NULL))abort();
#endif
 }
#ifdef _WIN32
 DeleteCriticalSection(&p->mutex);
#else
 if(pthread_cond_destroy(&p->changed)||pthread_mutex_destroy(&p->mutex))abort();
#endif
 free(p->snapshot);free(p);
}
int rs_async_prepare(RsAsync *a,const RsCore *core){
 if(!a||!core||!core->workspace||!core->samples||!core->config.height||
    core->samples>SIZE_MAX/2||core->samples!=(uint64_t)core->config.height*core->config.stride_samples||a->active)return 0;
 if(a->pool)return a->pool->height==core->config.height&&a->pool->stride==core->config.stride_samples;
 Pool *p=calloc(1,sizeof *p);if(!p)return 0;
 p->capacity=core->samples*2;p->snapshot=malloc(p->capacity);
 if(!p->snapshot){free(p);return 0;}
 /* 在曝光放行前触及实际内存页；不是读取或逐像素校验图像。 */
 volatile uint8_t *pages=(volatile uint8_t*)p->snapshot;
 for(size_t i=0;i<p->capacity;i+=4096)pages[i]=0;
 pages[p->capacity-1]=0;
 p->height=core->config.height;p->stride=core->config.stride_samples;
 p->count=p->height<4?p->height:4;
#ifdef _WIN32
 InitializeCriticalSection(&p->mutex);InitializeConditionVariable(&p->changed);
#else
 if(pthread_mutex_init(&p->mutex,NULL)){free(p->snapshot);free(p);return 0;}
 if(pthread_cond_init(&p->changed,NULL)){pthread_mutex_destroy(&p->mutex);free(p->snapshot);free(p);return 0;}
#endif
 for(unsigned i=0;i<p->count;i++){
  p->workers[i]=(Worker){p,i};
#ifdef RS_ASYNC_HOST_CHECK
  if(!rs_async_host_thread_allowed(i)){close_pool(p);return 0;}
#endif
#ifdef _WIN32
  p->threads[i]=CreateThread(NULL,0,work,p->workers+i,0,NULL);
  if(!p->threads[i]){close_pool(p);return 0;}
#else
  if(pthread_create(p->threads+i,NULL,work,p->workers+i)){close_pool(p);return 0;}
#endif
  p->created++;
 }
 a->pool=p;return 1;
}
int rs_async_start(RsAsync *a,RsCore *core,const uint16_t *source,size_t bytes){
 if(!a||a->active||!core||!source||!a->pool||a->pool->height!=core->config.height||
    a->pool->stride!=core->config.stride_samples||bytes<a->pool->capacity||!rs_begin_frame(core,bytes))return 0;
 Pool *p=a->pool;lock(&p->mutex);
 if(p->epoch==UINT64_MAX){unlock(&p->mutex);core->failed=1;return 0;}
 a->core=core;a->source=source;a->bytes=bytes;a->ok=0;a->active=1;
 a->source_read_ns=a->done_ns=0;a->started_ns=monotonic_ns();
 __atomic_store_n(&a->source_read,0,__ATOMIC_RELEASE);__atomic_store_n(&a->done,0,__ATOMIC_RELEASE);
 p->copied=p->finished=0;p->ok=1;p->task=a;p->epoch++;
 wake(&p->changed);unlock(&p->mutex);return 1;
}
int rs_async_source_read(const RsAsync *a){return a&&a->active&&__atomic_load_n(&a->source_read,__ATOMIC_ACQUIRE);}
int rs_async_timing(const RsAsync *a,RsAsyncTiming *t){
 if(!a||!t||!a->pool)return 0;
 *t=(RsAsyncTiming){0};
 t->copied=__atomic_load_n(&a->source_read,__ATOMIC_ACQUIRE);
 t->finished=__atomic_load_n(&a->done,__ATOMIC_ACQUIRE);
 if(t->copied&&a->started_ns&&a->source_read_ns>=a->started_ns)t->copy_ms=(a->source_read_ns-a->started_ns)/1000000;
 if(t->finished&&a->started_ns&&a->done_ns>=a->started_ns)t->total_ms=(a->done_ns-a->started_ns)/1000000;
 return 1;
}
int rs_async_collect(RsAsync *a,int *ok){
 if(!a||!ok||!a->active)return -1;
 if(!__atomic_load_n(&a->done,__ATOMIC_ACQUIRE))return 0;
 lock(&a->pool->mutex);*ok=a->ok;a->active=0;a->source=NULL;unlock(&a->pool->mutex);return 1;
}
int rs_async_shutdown(RsAsync *a){
 if(!a||a->active)return 0;
 if(a->pool){close_pool(a->pool);a->pool=NULL;}return 1;
}
