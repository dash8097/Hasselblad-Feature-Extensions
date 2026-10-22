/* 真实整幅NV16+生产适配核心；仅HAL、ION替身，无设备操作。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "quarter_upstream.h"
#include "factory_jpeg_layout.h"
#include "quarter_decision_wire.h"
typedef struct {uint8_t *p;uint32_t size;int mapped;} Memory;
static Memory source,output,*allocated;
static int allow,fail_alloc,fail_map,fail_sync,fail_unmap,fail_hal,bad_stream;
static int calls,small_calls,releases,source_unmaps,private_unmaps,which_map,which_sync,which_unmap;
static uint32_t rd(const void*p,unsigned off){uint32_t n;memcpy(&n,(const uint8_t*)p+off,4);return n;}
static void wr(void*p,unsigned off,uint32_t n){memcpy((uint8_t*)p+off,&n,4);}
static void pt(void*p,unsigned off,void*v){memcpy((uint8_t*)p+off,&v,sizeof v);}
static void *rp(const void*p,unsigned off){void*v;memcpy(&v,(const uint8_t*)p+off,sizeof v);return v;}
static int eligible(void*c,const void*f){(void)c;(void)f;return allow;}
static int size(void*m,uint32_t*n){*n=((Memory*)m)->size;return 0;}
static int map(void*m,void**p){if(++which_map==fail_map)return -1;Memory*b=m;assert(!b->mapped);b->mapped=1;*p=b->p;return 0;}
static int unmap(void*m){Memory*b=m;assert(b->mapped);if(b==&source)source_unmaps++;else private_unmaps++;b->mapped=0;return ++which_unmap==fail_unmap?-1:0;}
static int syncmem(void*m,int dir){assert(((Memory*)m)->mapped);assert(dir==1||dir==2);return ++which_sync==fail_sync?-1:0;}
static int allocate(void*c,void**p,uint32_t n){(void)c;if(fail_alloc)return -1;assert(!allocated);allocated=calloc(1,sizeof *allocated);assert(allocated);allocated->size=n;allocated->p=malloc(n);assert(allocated->p);*p=allocated;return 0;}
static int release(void*c,void*m){(void)c;assert(m==allocated&&!allocated->mapped);free(allocated->p);free(allocated);allocated=0;releases++;return 0;}
static int encode(void*c,QduRequest*r){
 (void)c;calls++;
 if(rd(r->frame,0x38)==11656){assert(r->quality==90);return 0;}
 small_calls++;assert(r->quality==80&&rd(r->frame,0x38)==5828&&rd(r->frame,0x3c)==4372);
 assert(rd(r->stream,0x30)==5828&&rd(r->stream,0x34)==4372&&rd(r->stream,0x90)==0xabcdef);
 assert(rp(r->stream,0x20)==&output&&rp(r->frame,0x20)==allocated);
 assert(!source.mapped&&!allocated->mapped);
 assert(rd(r->frame,0x40)==5888&&rd(r->frame,0x80)==2);
 for(unsigned plane=0;plane<2;plane++){
  const uint8_t*p=allocated->p+plane*5888u*4372;
  for(unsigned row=0;row<4372;row++)for(unsigned x=0;x<5828;x++)
   assert(p[row*5888u+x]==(plane?128:37));
 }
 if(fail_hal)return -77;
 wr(r->stream,0x40,bad_stream?output.size+1:8192);return 0;
}
static const QduOps ops={0,eligible,size,map,unmap,syncmem,allocate,release,encode};
static void reset(void){allow=1;fail_alloc=fail_map=fail_sync=fail_unmap=fail_hal=bad_stream=0;calls=small_calls=releases=source_unmaps=private_unmaps=which_map=which_sync=which_unmap=0;assert(!allocated&&!source.mapped);}
static void descriptors(uint8_t*f,uint8_t*s){factory_jpeg_layout(f,s,&source,&output,output.size);wr(s,0x90,0xabcdef);}
static unsigned tests;
static void run_case(const char *name,int expected_rc,int direct,int freed,int blocked){
 uint8_t f[0x218],s[0x1c8],before[0x218];descriptors(f,s);memcpy(before,f,sizeof f);
 QduState state={0};QduRequest req={f,s,90,0};
 assert(qdu_encode(&state,&ops,0,&req)==expected_rc);
 assert(!memcmp(before,f,sizeof f)&&req.quality==90&&calls==1&&small_calls==direct&&releases==freed&&state.blocked==(unsigned)blocked);
 if(state.quarantined){assert(state.quarantined==allocated);int count=calls;assert(qdu_encode(&state,&ops,0,&req)==0&&calls==count+1);assert(small_calls==direct);release(0,allocated);}
 assert(!allocated&&!source.mapped);tests++;printf("PASS %s\n",name);
}
static void mapping(void){
 uint8_t engine[0x230]={0},mapdata[0x30]={0},root[0xc0]={0},left[0xc0]={0},right[0xc0]={0},functor[0x30]={0};
 pt(engine,0x220,mapdata);pt(mapdata,0x10,root);wr(root,0x20,11);pt(root,0,left);pt(root,8,right);
 wr(left,0x20,3);wr(right,0x20,90);pt(right,0x70,functor);pt(functor,0,(void*)0x12345678);wr(functor,0x24,123);
 uint32_t id=0;assert(qdu_exposure(engine,90,0x12345678,&id)&&id==123);
 assert(!qdu_exposure(engine,90,0x87654321,&id));assert(!qdu_exposure(engine,89,0x12345678,&id));
 pt(left,0,left);assert(!qdu_exposure(engine,1,0x12345678,&id)); /* bounded corrupt tree */
 QdDecision q={QD_DECISION_MAGIC,123,1,1};assert(qd_decision_valid(&q,123));
 assert(!qd_decision_valid(&q,124));q.known=0;assert(!qd_decision_valid(&q,123));q.known=1;q.quarter=0;assert(!qd_decision_valid(&q,123));
 tests++;puts("PASS native_qmap_and_snapshot_identity");
}
int main(void){
 source.size=11776u*8742*2;source.p=malloc(source.size);assert(source.p);
 memset(source.p,37,source.size/2);memset(source.p+source.size/2,128,source.size/2);output.size=65536;
 reset();run_case("direct_25mp_q80_once",0,1,1,0);
 reset();allow=0;run_case("ordinary_or_unknown_snapshot_fallback",0,0,0,0);
 reset();fail_alloc=1;run_case("allocation_failure_fallback",0,0,0,0);
 reset();fail_map=1;run_case("source_map_failure",0,0,1,0);
 reset();fail_map=2;run_case("private_map_failure",0,0,1,0);
 reset();fail_sync=1;run_case("source_sync_failure",0,0,1,0);
 reset();fail_sync=2;run_case("private_sync_failure",0,0,1,0);
 reset();fail_unmap=1;run_case("source_unmap_no_retry",0,0,1,1);assert(source_unmaps==1);
 reset();fail_unmap=2;run_case("private_unmap_quarantine",0,0,0,1);assert(private_unmaps==1);
 reset();fail_hal=1;run_case("hal_error_quarantine_no_retry",-77,1,0,1);
 reset();bad_stream=1;run_case("invalid_output_no_second_encode",-1001,1,1,0);
 reset();
 {uint8_t f[0x218],s[0x1c8];descriptors(f,s);wr(f,0x50,11776-256);QduRequest r={f,s,90,0};QduState st={0};assert(!qdu_encode(&st,&ops,0,&r)&&!small_calls&&!allocated);tests++;puts("PASS inconsistent_planes_fallback");}
 mapping();
 for(uint32_t i=0;i<source.size;i++)assert(source.p[i]==(i<source.size/2?37:128));free(source.p);
 printf("QUARTER_UPSTREAM_HOST_PASS %u source_pixels_unchanged\n",tests);return 0;
}
