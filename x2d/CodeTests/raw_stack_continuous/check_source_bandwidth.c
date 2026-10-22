/* 4.2.0 自有 ION 缓冲基准，不访问原厂帧，不拍摄。 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <arm_neon.h>
#include <unistd.h>
extern int duss_hal_mem_dma_request_chan(void*,uint32_t,void**),duss_hal_mem_dma_submit(void*,void*,void*,uint64_t,void*,uint64_t,uint64_t,void(*)(void*),void*),duss_hal_mem_dma_issue(void*,void*),duss_hal_mem_dma_terminate(void*,void*),duss_hal_mem_dma_release_chan(void*,void*);
static void completed(void *p){__atomic_store_n((int*)p,1,__ATOMIC_RELEASE);}
extern int duss_hal_mem_sync(void*,uint32_t);
extern int duss_hal_initialize(void*),duss_hal_deinitialize(void);
extern int duss_hal_attach_ion_mem(const char*,void**),duss_hal_detach_ion_mem(void*);
extern int duss_hal_device_open(const char*,void*,void**),duss_hal_device_start(void*,void*),duss_hal_device_stop(void*,void*),duss_hal_device_close(void*);
extern int duss_hal_mem_alloc(void*,void**,uint32_t,uint32_t,uint32_t,uint32_t),duss_hal_mem_map(void*,void**),duss_hal_mem_unmap(void*),duss_hal_mem_free(void*);
struct module {const char *name;int(*attach)(const char*,void**);int(*detach)(void*);void *handle;};
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
__attribute__((noinline)) static void old(uint16_t *d,const uint16_t *s,size_t n){for(size_t i=0;i<n;i+=8)vst1q_u16(d+i,vminq_u16(vld1q_u16(d+i),vld1q_u16(s+i)));}
__attribute__((noinline)) static void grouped(uint16_t *d,const uint16_t *s,size_t n){for(size_t i=0;i<n;i+=32){uint16x8x4_t a=vld1q_u16_x4(s+i),b=vld1q_u16_x4(d+i);for(unsigned k=0;k<4;k++)b.val[k]=vminq_u16(a.val[k],b.val[k]);vst1q_u16_x4(d+i,b);}}
static int run(int argc,char **argv,char **env){(void)argc;(void)argv;(void)env;
 struct module m[]={{"/dev/ion",duss_hal_attach_ion_mem,duss_hal_detach_ion_mem,0},{0}};
 void *dev=0,*mem=0,*src=0,*other=0,*other_map=0,*channel=0;uint16_t *dst=0;int initialized=0,started=0,config=0,rc=1;const size_t bytes=210510336,n=bytes/2;
 if(duss_hal_initialize(m))goto done;initialized=1;
 if(duss_hal_device_open("/dev/ion",&config,&dev)||!dev)goto done;
 if(duss_hal_device_start(dev,&config))goto done;started=1;
 if(duss_hal_mem_alloc(dev,&mem,bytes,1024,2,0x10)||!mem||duss_hal_mem_map(mem,&src)||!src)goto done;
 dst=malloc(bytes);if(!dst)goto done;
 uint16x8_t v=vdupq_n_u16(12345);for(size_t i=0;i<n;i+=8)vst1q_u16((uint16_t*)src+i,v);
 if(duss_hal_mem_alloc(dev,&other,bytes,1024,2,0x10)||!other||duss_hal_mem_map(other,&other_map)||!other_map)goto done;
 for(size_t i=0;i<n;i+=8)vst1q_u16((uint16_t*)other_map+i,vdupq_n_u16(54321));
 old(dst,src,n);grouped(dst,src,n);
 int status=duss_hal_mem_dma_request_chan(dev,0,&channel);printf("DMA_REQUEST rc=%d channel=%d\n",status,channel!=0);fflush(stdout);
 if(status||!channel)goto done;
 if(duss_hal_mem_sync(mem,2)||duss_hal_mem_sync(other,2))goto done;
 uint64_t total=ns();
 for(size_t offset=0;offset<bytes;){
 size_t chunk=bytes-offset;if(chunk>16*1024*1024)chunk=16*1024*1024;
 int completion=0;uint64_t t=ns();status=duss_hal_mem_dma_submit(dev,channel,mem,offset,other,offset,chunk,completed,&completion);
 printf("DMA_SUBMIT offset=%zu bytes=%zu rc=%d\n",offset,chunk,status);fflush(stdout);
 if(!status)status=duss_hal_mem_dma_issue(dev,channel);
 while(!status&&!__atomic_load_n(&completion,__ATOMIC_ACQUIRE)&&ns()-t<5000000000ull)usleep(1000);
 printf("DMA_FINISH issue=%d complete=%d ns=%llu\n",status,completion,(unsigned long long)(ns()-t));fflush(stdout);
 if(!completion){int term=duss_hal_mem_dma_terminate(dev,channel);printf("DMA_TERMINATE rc=%d\n",term);fflush(stdout);if(term){for(;;)sleep(60);}goto done;}
 offset+=chunk;
 }
 printf("DMA_FULL_COPY ns=%llu\n",(unsigned long long)(ns()-total));
 if(duss_hal_mem_sync(mem,1))goto done;
 uint16_t first=((uint16_t*)src)[0],second=((uint16_t*)other_map)[0];
 if(first!=second||(first!=12345&&first!=54321))goto done;
 for(size_t i=0;i<n;i++)if(((uint16_t*)src)[i]!=first||((uint16_t*)other_map)[i]!=first)goto done;
 printf("DMA_VERIFIED value=%u bytes=%zu\n",first,bytes);rc=0;
done:
 if(channel&&duss_hal_mem_dma_release_chan(dev,channel))rc=7;
 if(other_map&&duss_hal_mem_unmap(other))rc=8;if(other&&duss_hal_mem_free(other))rc=9;
 free(dst);if(src&&duss_hal_mem_unmap(mem))rc=2;if(mem&&duss_hal_mem_free(mem))rc=3;
 if(started&&duss_hal_device_stop(dev,0))rc=4;if(dev&&duss_hal_device_close(dev))rc=5;if(initialized&&duss_hal_deinitialize())rc=6;
 printf("OWNED_BUFFER_RELEASE rc=%d\n",rc);return rc;
}
typedef struct {void (**preinit)(void),(**init)(void),(**fini)(void);} Structors;
extern __attribute__((noreturn))void __libc_init(void*,void(*)(void),int(*)(int,char**,char**),const Structors*);
__attribute__((noreturn,used))void entry(void *s){const Structors e={0};__libc_init(s,0,run,&e);}
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0, sp\nbl entry");}



