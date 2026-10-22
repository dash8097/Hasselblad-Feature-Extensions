#include "raw_stack_dma.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int fault,freed,unmapped,released,submits,terminated;static uint64_t clock_value;static void(*cb)(void*);static void*context;
static int allocate(void*d,void**o,uint32_t n,uint32_t a,uint32_t h,uint32_t f){assert(d==(void*)1&&n==210510336&&a==1024&&h==2&&f==0x10);if(fault==1)return -1;*o=(void*)2;return 0;}
static int size(void*p,uint32_t*n){assert(p==(void*)2||p==(void*)4);if(fault==2)return -1;*n=210510336;return 0;}
static int map(void*p,void**o){assert(p==(void*)2);if(fault==3)return -1;*o=(void*)3;return 0;}
static int unmap(void*p){assert(p==(void*)2);unmapped++;return 0;}
static int free_mem(void*p){assert(p==(void*)2);freed++;return 0;}
static int sync_mem(void*p,uint32_t f){assert(p==(void*)2&&(f==1||f==2));return (fault==5&&f==2)||(fault==10&&f==1)?-1:0;}
static int request(void*d,uint32_t f,void**p){assert(d==(void*)1&&f==0);if(fault==4)return -1;*p=(void*)5;return 0;}
static int submit(void*d,void*c,void*dst,uint64_t off,void*src,uint64_t so,uint64_t n,void(*fn)(void*),void*x){assert(d==(void*)1&&c==(void*)5&&dst==(void*)2&&src==(void*)4&&off==so&&n<=16777216&&off==(uint64_t)submits*16777216);submits++;cb=fn;context=x;return fault==6?-1:0;}
static int issue(void*d,void*c){assert(d==(void*)1&&c==(void*)5);if(fault==7)return -1;if(fault!=8&&fault!=9)cb(context);return 0;}
static int terminate(void*d,void*c){assert(d==(void*)1&&c==(void*)5);terminated++;return fault==9?-1:0;}
static int release(void*d,void*c){assert(d==(void*)1&&c==(void*)5);released++;return 0;}
static uint64_t clock_ms(void){return clock_value;}
static void pause_ms(void){clock_value+=1000;}
static RsDmaApi api={allocate,map,unmap,free_mem,size,sync_mem,request,submit,issue,terminate,release,clock_ms,pause_ms};
int main(void){unsigned cases=0;for(fault=0;fault<=10;fault++){
 RsDma d={0};freed=unmapped=released=submits=terminated=0;clock_value=0;
 int ready=rs_dma_prepare(&d,api,(void*)1,210510336);assert(ready==(fault==0||fault>=5));
 if(ready){int copied=rs_dma_copy(&d,(void*)4,0,210510336);assert(copied==(fault==0));if(!fault)assert(submits==13&&!terminated);}
 if(fault==9){assert(d.uncertain&&!rs_dma_close(&d)&&!freed&&!unmapped&&!released);d.uncertain=0;}
 assert(rs_dma_close(&d));assert(freed==(fault!=1));assert(unmapped==(fault==0||fault>=4));assert(released==(fault==0||fault>=5));cases++;
 }
 fault=0;RsDma d={0};assert(rs_dma_prepare(&d,api,(void*)1,210510336));submits=0;
 assert(!rs_dma_copy(&d,(void*)4,32,210510336));assert(!rs_dma_copy(&d,(void*)4,1,32));assert(!rs_dma_copy(&d,(void*)4,0,0));assert(!submits);assert(rs_dma_close(&d));cases+=3;
 printf("DMA_OWNERSHIP_TIMEOUT_CHUNK_BOUNDS_PASS cases=%u\n",cases);return 0;}
