#include "raw_stack_core.h"
#include "raw_stack_async.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include "raw_stack_dma.h"
#include <string.h>
static RsDma dma_snapshot;
static int (*mem_allocate)(void*,void**,uint32_t,uint32_t,uint32_t,uint32_t);
static int (*mem_map)(void*,void**),(*mem_unmap)(void*),(*mem_free)(void*),(*mem_size)(void*,uint32_t*);
#define RTLD_DEFAULT 0
static void *dlsym(int a,const char*b){(void)a;(void)b;return (void*)1;}
static uint64_t now_ms(void){return 1;}
static void dma_pause(void){}
static int mock_dma_prepare(RsDma*d,RsDmaApi a,void*i,size_t n){(void)a;(void)i;d->capacity=n;return 1;}
static int mock_dma_close(RsDma*d){*d=(RsDma){0};return 1;}
#define rs_dma_prepare mock_dma_prepare
#define rs_dma_close mock_dma_close
static RsCore core;static RsAsync accumulating;static RsConfig pending_config;
static void *ion,*pool_module,*accumulator[RS_MAX_SEGMENTS],*mapped[RS_MAX_SEGMENTS];
static int failed,pool_owned,pool_api,live,attempt,fail_at,budget_calls,budget_fail;
static size_t accumulator_bytes,first_budget;
static void log_line(const char *s){assert(s);}
static int snapshot_budget(size_t n){if(!budget_calls)first_budget=n;return ++budget_calls!=budget_fail;}
static int rs_pool_close(int api,void **i,void **m,int *owned){(void)api;*i=*m=NULL;*owned=0;return 1;}
static void *test_malloc(size_t n){if(++attempt==fail_at)return NULL;void*p=malloc(n);if(p)live++;return p;}
static void test_free(void*p){if(p)live--;free(p);}
static int ion_live,ion_step,ion_fail,release_fail;
typedef struct {void*p;uint32_t bytes;} MockIon;
static int alloc_ion(void*d,void**h,uint32_t n,uint32_t a,uint32_t heap,uint32_t flags){
 assert(d&&a==1024&&heap==2&&flags==0x10);if(++ion_step==ion_fail)return -1;
 MockIon*m=calloc(1,sizeof *m);assert(m);m->p=malloc(n);assert(m->p);m->bytes=n;*h=m;ion_live++;return 0;
}
static int size_ion(void*h,uint32_t*n){if(++ion_step==ion_fail)return -1;*n=((MockIon*)h)->bytes;return 0;}
static int map_ion(void*h,void**p){if(++ion_step==ion_fail)return -1;*p=((MockIon*)h)->p;return 0;}
static int unmap_ion(void*h){assert(h);return release_fail==1?-1:0;}
static int free_ion(void*h){if(release_fail==2)return -1;MockIon*m=h;free(m->p);free(m);ion_live--;return 0;}
#define malloc test_malloc
#define free test_free
#define RS_ASYNC_DIRECT 1
/* PRODUCTION_CPU_ALLOCATION */

int main(void){mem_allocate=alloc_ion;mem_size=size_ion;mem_map=map_ion;mem_unmap=unmap_ion;mem_free=free_ion;unsigned cases=0;for(int f=0;f<=RS_MAX_SEGMENTS;f++)for(int budget=0;budget<3;budget++){
 assert(!live&&!ion_live);core=(RsCore){0};accumulating=(RsAsync){0};pending_config=(RsConfig){.width=17,.height=RS_MAX_SEGMENTS,.stride_samples=17,.method=RS_MINIMUM};assert(rs_workspace_bytes(pending_config,&accumulator_bytes));ion=(void*)1;attempt=budget_calls=0;budget_fail=budget;fail_at=f;int ok=pool_allocate();assert(first_budget==accumulator_bytes);assert(ok==(budget==1||(budget==0&&f==0)));assert(pool_release());assert(!live&&!ion&&!ion_live&&!accumulating.pool);cases++;}
 for(int f=0;f<=RS_MAX_SEGMENTS*3;f++)for(int r=0;r<3;r++){
  core=(RsCore){0};accumulating=(RsAsync){0};pending_config.method=RS_AVERAGE;assert(rs_workspace_bytes(pending_config,&accumulator_bytes));ion=(void*)1;
  budget_calls=0;budget_fail=1;ion_step=0;ion_fail=f;release_fail=0;int ok=pool_allocate();assert(ok==(f==0));
  release_fail=r;int closed=pool_release();if(ion_live){assert(!closed&&ion);release_fail=0;assert(pool_release());}
  assert(!ion_live&&!live&&!ion);cases++;
 }
 printf("CPU_ACCUMULATOR_ALLOCATION_RELEASE_PASS cases=%u\n",cases);}
