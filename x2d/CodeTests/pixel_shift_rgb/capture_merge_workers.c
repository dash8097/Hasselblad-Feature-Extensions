#include <stdlib.h>
#include "capture_merge_workers.h"
#include "factory_ion_copy.h"
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION Lock;
typedef CONDITION_VARIABLE Condition;
typedef HANDLE Thread;
#define ENTRY DWORD WINAPI
#else
#include <pthread.h>
#include <time.h>
typedef pthread_mutex_t Lock;
typedef pthread_cond_t Condition;
typedef pthread_t Thread;
#define ENTRY void *
#ifdef X2D_CAPTURE_BIONIC_PROFILE
_Static_assert(sizeof(Lock)>=40&&_Alignof(Lock)>=8,"Bionic mutex storage");
_Static_assert(sizeof(Condition)>=4&&_Alignof(Condition)>=4,"Bionic condition storage");
#endif
#endif
struct CmwWorkers {
 Lock lock;Condition changed;Thread threads[4];
 struct {struct CmwWorkers *pool;unsigned id;void *scratch;} slots[4];
 unsigned width,started,generation,completed,stop,ok;
 unsigned capacity,stride,row0,span,copying;uint8_t *cache;
 uint64_t read_ms,compute_ms;
 CmView frames[6],sources[6];unsigned height,first,rows;uint16_t *output;
};
static uint64_t now_ms(void){
#ifdef _WIN32
 return GetTickCount64();
#else
 struct timespec t;return clock_gettime(CLOCK_MONOTONIC,&t)?0:(uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
#endif
}
static unsigned cache_stride(unsigned width){return width==11663?23808:(width+126)*2;}
size_t cmw_buffer_bytes(unsigned width,unsigned rows){
 if(!cm_merge_scratch_bytes(width)||!rows||rows>1024)return 0;
 return (size_t)cache_stride(width)*(rows+3)*6+4*cm_merge_scratch_bytes(width);
}
uint64_t cmw_read_ms(const CmwWorkers *p){return p?p->read_ms:0;}
unsigned cmw_capacity(const CmwWorkers *p){return p?p->capacity:0;}
uint64_t cmw_compute_ms(const CmwWorkers *p){return p?p->compute_ms:0;}
static void lock(CmwWorkers *p){
#ifdef _WIN32
 EnterCriticalSection(&p->lock);
#else
 pthread_mutex_lock(&p->lock);
#endif
}
static void unlock(CmwWorkers *p){
#ifdef _WIN32
 LeaveCriticalSection(&p->lock);
#else
 pthread_mutex_unlock(&p->lock);
#endif
}
static void wake(CmwWorkers *p){
#ifdef _WIN32
 WakeAllConditionVariable(&p->changed);
#else
 pthread_cond_broadcast(&p->changed);
#endif
}
static void wait_changed(CmwWorkers *p){
#ifdef _WIN32
 if(!SleepConditionVariableCS(&p->changed,&p->lock,INFINITE))abort();
#else
 if(pthread_cond_wait(&p->changed,&p->lock))abort();
#endif
}
static ENTRY compute(void *opaque){
 CmwWorkers *p=((typeof(((CmwWorkers*)0)->slots[0])*)opaque)->pool;
 unsigned id=((typeof(p->slots[0])*)opaque)->id,seen=0;
 lock(p);
 for(;;){
  while(!p->stop&&seen==p->generation)wait_changed(p);
  if(p->stop)break;
  seen=p->generation;unsigned first=p->rows*id/4,end=p->rows*(id+1)/4;
  unlock(p);
  int ok=1;
  if(p->copying){
   /* 把六帧窗口的连续行均分给四个持久线程；目的范围互不重叠。
    * 全部复制结束后才进入计算，保留跨线程行段所需的周边像素。 */
   unsigned begin=6*p->span*id/4,limit=6*p->span*(id+1)/4;
   while(begin<limit){
    unsigned frame=begin/p->span,y=begin%p->span,n=p->span-y;
    if(n>limit-begin)n=limit-begin;
    uint8_t *to=p->cache+((size_t)frame*(p->capacity+3)+y)*p->stride;
    const uint8_t *from=p->sources[frame].data+(size_t)(p->row0+y)*p->sources[frame].stride;
    for(unsigned line=0;line<n;line++){
#ifdef __aarch64__
     factory_ion_copy(to,from,p->stride);
#else
     memcpy(to,from,p->stride);
#endif
     to+=p->stride;from+=p->sources[frame].stride;
    }
    begin+=n;
   }
  }else ok=first==end||cm_merge_cached_rows(p->frames,p->row0,p->span,p->width,p->height,p->first+first,end-first,
   p->output+(size_t)first*p->width*4,(size_t)(end-first)*p->width*8,
   p->slots[id].scratch,cm_merge_scratch_bytes(p->width));
  lock(p);p->ok&=(unsigned)ok;p->completed++;wake(p);
 }
 unlock(p);return 0;
}
void cmw_destroy(CmwWorkers *p){
 if(!p)return;
 lock(p);p->stop=1;wake(p);unlock(p);
 for(unsigned i=0;i<p->started;i++){
#ifdef _WIN32
  WaitForSingleObject(p->threads[i],INFINITE);CloseHandle(p->threads[i]);
#else
  pthread_join(p->threads[i],NULL);
#endif
 }
 for(unsigned i=0;i<4;i++)free(p->slots[i].scratch);
#ifdef _WIN32
 DeleteCriticalSection(&p->lock);
#else
 pthread_cond_destroy(&p->changed);pthread_mutex_destroy(&p->lock);
#endif
 free(p->cache);free(p);
}
CmwWorkers *cmw_create(unsigned width){
 return cmw_create_rows(width,1024);
}
CmwWorkers *cmw_create_rows(unsigned width,unsigned rows){
 if(!cmw_buffer_bytes(width,rows))return NULL;
 CmwWorkers *p=calloc(1,sizeof *p);if(!p)return NULL;p->width=width;
 p->capacity=rows;p->stride=cache_stride(width);
#ifdef _WIN32
 InitializeCriticalSection(&p->lock);InitializeConditionVariable(&p->changed);
#else
 if(pthread_mutex_init(&p->lock,NULL)){free(p);return NULL;}
 if(pthread_cond_init(&p->changed,NULL)){pthread_mutex_destroy(&p->lock);free(p);return NULL;}
#endif
 p->cache=malloc((size_t)p->stride*(rows+3)*6);if(!p->cache)goto failed;
 for(unsigned i=0;i<4;i++){
  p->slots[i].pool=p;p->slots[i].id=i;
  p->slots[i].scratch=malloc(cm_merge_scratch_bytes(width));if(!p->slots[i].scratch)goto failed;
#ifdef _WIN32
  p->threads[i]=CreateThread(NULL,0,compute,&p->slots[i],0,NULL);if(!p->threads[i])goto failed;
#else
  if(pthread_create(&p->threads[i],NULL,compute,&p->slots[i]))goto failed;
#endif
  p->started++;
 }
 return p;
failed:cmw_destroy(p);return NULL;
}
int cmw_run(CmwWorkers *p,const CmView frames[6],unsigned height,unsigned first,unsigned rows,
 uint16_t *output,size_t bytes){
 if(!p||!frames||!output||!rows||rows>p->capacity||!height||height>8749||first>=height||
    rows>height-first||bytes!=(size_t)p->width*rows*8)return 0;
 unsigned row0=first?first+91:92;
 unsigned last=(first+rows<height?first+rows:height-1)+93;
 unsigned span=last-row0+1;
 if(span>p->capacity+3||last>=8842)return 0;
 for(unsigned i=0;i<6;i++)if(x2d_capture_raw_geometry(frames[i].width,frames[i].height,
    frames[i].format,frames[i].stride)!=X2D_CAPTURE_RAW_VALID||!frames[i].data||
    ((uintptr_t)frames[i].data&1)||frames[i].owns_allocation||frames[i].bytes<210510336)return 0;
 uint64_t start=now_ms();
 lock(p);p->row0=row0;p->span=span;p->height=height;p->first=first;
 p->rows=rows;p->output=output;
 /* 所有源引用仍由协调层持有，复制和计算共用四个线程，避免超额订阅。 */
 for(unsigned i=0;i<6;i++){
  uint8_t *to=p->cache+(size_t)i*(p->capacity+3)*p->stride;
  p->sources[i]=frames[i];
  p->frames[i]=frames[i];p->frames[i].data=to;p->frames[i].stride=p->stride;p->frames[i].bytes=(size_t)span*p->stride;
 }
 p->copying=1;p->completed=0;p->ok=1;p->generation++;wake(p);
 while(p->completed<4)wait_changed(p);
 uint64_t copied=now_ms();if(copied>=start)p->read_ms+=copied-start;
 memset(p->sources,0,sizeof p->sources);
 p->copying=0;p->completed=0;p->ok=1;p->generation++;wake(p);
 while(p->completed<4)wait_changed(p);
 uint64_t done=now_ms();if(done>=copied)p->compute_ms+=done-copied;
 int ok=p->ok;memset(p->frames,0,sizeof p->frames);p->output=NULL;unlock(p);return ok;
}
