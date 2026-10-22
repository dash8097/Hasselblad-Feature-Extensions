/* 只用小型自造数据调用候选库的实际常驻工人，不启用拍摄钩子。 */
#include "raw_stack_async.h"
#include <time.h>
#include <unistd.h>
#include <string.h>
static uint64_t milliseconds(void){
 struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
 return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
static int check(int argc,char **argv,char **envp){
 (void)argc;(void)argv;(void)envp;
 RsCore core;RsAsync a={0};_Alignas(8) uint32_t sums[64];uint16_t source[64];
 if(!rs_init(&core,(RsConfig){8,8,8,RS_AVERAGE,0},sums,sizeof sums)||!rs_async_prepare(&a,&core))return 11;
 struct RsAsyncPool *pool=a.pool;
 for(unsigned n=1;n<=32;n++){
  for(unsigned i=0;i<64;i++)source[i]=(uint16_t)n;
  if(!rs_async_start(&a,&core,source,sizeof source))return 12;
  uint64_t deadline=milliseconds()+3000;int ok=0,collected=0;
  do{
   if(rs_async_collect(&a,&ok)==1){collected=1;break;}
   struct timespec delay={0,1000000};nanosleep(&delay,0);
  }while(milliseconds()<deadline);
  if(!collected||!ok||core.frames!=n||a.pool!=pool)return 13;
 }
 for(unsigned i=0;i<64;i++)if(sums[i]!=528)return 14;
 if(!rs_async_shutdown(&a)||a.pool)return 15;
 const char *passed="RAW_STACK_BIONIC_PERSISTENT_FOUR_WORKERS_32_FRAMES_PASSED_NO_CAPTURE\n";
 return write(1,passed,strlen(passed))==(ssize_t)strlen(passed)?0:16;
}
typedef struct {void (**preinit)(void),(**init)(void),(**fini)(void);} Structors;
extern __attribute__((noreturn))void __libc_init(void *,void (*)(void),int (*)(int,char**,char**),const Structors *);
__attribute__((noreturn,used))void stack_entry(void *stack){const Structors empty={0};__libc_init(stack,0,check,&empty);}
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0, sp\nbl stack_entry");}
