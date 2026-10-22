/* 双路读者真实重叠时的所有权检查；仅主机数据，无相机或文件操作。 */
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION pthread_mutex_t;
typedef HANDLE pthread_t;
typedef struct {void *(*run)(void *);void *context;} TestThread;
static int pthread_mutex_init(pthread_mutex_t *m,void *unused){(void)unused;InitializeCriticalSection(m);return 0;}
static int pthread_mutex_lock(pthread_mutex_t *m){EnterCriticalSection(m);return 0;}
static int pthread_mutex_unlock(pthread_mutex_t *m){LeaveCriticalSection(m);return 0;}
static int pthread_mutex_destroy(pthread_mutex_t *m){DeleteCriticalSection(m);return 0;}
static DWORD WINAPI test_thread(void *opaque){TestThread t=*(TestThread*)opaque;free(opaque);t.run(t.context);return 0;}
static int pthread_create(pthread_t *p,void *unused,void *(*run)(void *),void *context){
 (void)unused;TestThread *t=malloc(sizeof *t);if(!t)return 1;
 *t=(TestThread){run,context};*p=CreateThread(NULL,0,test_thread,t,0,NULL);
 if(!*p){free(t);return 1;}return 0;
}
static int pthread_join(pthread_t p,void *unused){(void)unused;return WaitForSingleObject(p,INFINITE)!=WAIT_OBJECT_0||!CloseHandle(p);}
#else
#include <pthread.h>
#endif
#include "pixel_chunk_fanout.h"
#define CHECK(v) do{if(!(v)){fprintf(stderr,"CHUNK_FANOUT_CHECK_FAILED line=%u\n",__LINE__);exit(1);}}while(0)
typedef struct {unsigned used,released;uint16_t values[6];} Owned;
typedef struct {PcFanout p;pthread_mutex_t lock;unsigned releases;} Test;
static void release(void *context,void *owner){
 Test *t=context;Owned *o=owner;CHECK(o->used&&!o->released);o->released=1;t->releases++;
}
static void init(Test *t,unsigned height){
 memset(t,0,sizeof *t);CHECK(!pthread_mutex_init(&t->lock,NULL));
 CHECK(pc_init(&t->p,7,3,height,release,t));
}
static void ordered_and_stale(void){
 Test t;init(&t,10);Owned owned[5]={0};PcChunk raw,render;
 for(unsigned i=0;i<4;i++){owned[i].used=1;CHECK(pc_submit(&t.p,i*2,2,owned[i].values,12,owned+i));}
 owned[4].used=1;CHECK(!pc_submit(&t.p,8,2,owned[4].values,12,owned+4));
 CHECK(pc_take(&t.p,PC_RAW,&raw)&&pc_take(&t.p,PC_RENDER,&render));
 CHECK(raw.ticket==render.ticket&&!pc_take(&t.p,PC_RAW,&raw));
 CHECK(!pc_done(&t.p,PC_RAW,8,raw.ticket,1));
 CHECK(pc_done(&t.p,PC_RAW,7,raw.ticket,1));CHECK(!t.releases);
 CHECK(!pc_submit(&t.p,8,2,owned[4].values,12,owned+4));
 CHECK(pc_done(&t.p,PC_RENDER,7,render.ticket,1));CHECK(t.releases==1);
 CHECK(pc_submit(&t.p,8,2,owned[4].values,12,owned+4));
 CHECK(!pc_done(&t.p,PC_RAW,7,raw.ticket,1));
 for(unsigned row=2;row<10;row+=2){
  CHECK(pc_take(&t.p,PC_RENDER,&render)&&render.first_row==row);
  CHECK(pc_take(&t.p,PC_RAW,&raw)&&raw.first_row==row);
  CHECK(pc_done(&t.p,PC_RENDER,7,render.ticket,1));
  CHECK(pc_done(&t.p,PC_RAW,7,raw.ticket,1));
 }
 CHECK(pc_complete(&t.p)&&t.releases==5);CHECK(!pthread_mutex_destroy(&t.lock));
}
static void cancellation(void){
 Test t;init(&t,6);Owned o[3]={0};PcChunk a,b;
 for(unsigned i=0;i<3;i++){o[i].used=1;CHECK(pc_submit(&t.p,2*i,2,o[i].values,12,o+i));}
 CHECK(pc_take(&t.p,0,&a)&&pc_take(&t.p,1,&b));pc_abort(&t.p);
 CHECK(t.releases==2&&!o[0].released&&!pc_drained(&t.p));
 CHECK(!pc_take(&t.p,0,&a));CHECK(pc_done(&t.p,0,7,a.ticket,0));
 CHECK(!o[0].released);CHECK(pc_done(&t.p,1,7,b.ticket,1));
 CHECK(t.releases==3&&pc_drained(&t.p)&&!pc_complete(&t.p));
 CHECK(!pthread_mutex_destroy(&t.lock));
}
typedef struct {Test *test;unsigned consumer;uint64_t sum;} Consumer;
static void *consume(void *context){
 Consumer *c=context;Test *t=c->test;
 for(;;){
  PcChunk chunk;CHECK(!pthread_mutex_lock(&t->lock));
  int got=pc_take(&t->p,c->consumer,&chunk);
  int done=pc_complete(&t->p);
  CHECK(!pthread_mutex_unlock(&t->lock));
  if(done)return NULL;if(!got)continue;
  Owned *o=chunk.owner;CHECK(!o->released);
  const uint16_t *p=chunk.pixels;
  for(unsigned i=0;i<chunk.bytes/2;i++)c->sum+=p[i];
  CHECK(!pthread_mutex_lock(&t->lock));CHECK(!o->released);
  CHECK(pc_done(&t->p,c->consumer,chunk.task,chunk.ticket,1));
  CHECK(!pthread_mutex_unlock(&t->lock));
 }
}
static void parallel(void){
 enum{N=4000};Test t;init(&t,2*N);Owned *o=calloc(N,sizeof *o);CHECK(o);
 Consumer consumers[2]={{&t,0,0},{&t,1,0}};pthread_t threads[2];
 for(unsigned i=0;i<2;i++)CHECK(!pthread_create(threads+i,NULL,consume,consumers+i));
 uint64_t expected=0;
 for(unsigned n=0;n<N;n++){
  o[n].used=1;for(unsigned i=0;i<6;i++){o[n].values[i]=(uint16_t)(n+i);expected+=o[n].values[i];}
  for(;;){
   CHECK(!pthread_mutex_lock(&t.lock));int accepted=pc_submit(&t.p,2*n,2,o[n].values,12,o+n);
   CHECK(!pthread_mutex_unlock(&t.lock));if(accepted)break;
  }
 }
 for(unsigned i=0;i<2;i++)CHECK(!pthread_join(threads[i],NULL));
 CHECK(pc_complete(&t.p)&&t.releases==N);
 CHECK(consumers[0].sum==expected&&consumers[1].sum==expected);
 free(o);CHECK(!pthread_mutex_destroy(&t.lock));
}
int main(void){ordered_and_stale();cancellation();parallel();
 puts("DUAL_CONSUMER_ORDER_BACKPRESSURE_REFERENCE_LIFETIME_AND_FAILURE_DRAIN_PASSED_NOT_CONNECTED_TO_CAMERA");return 0;}
