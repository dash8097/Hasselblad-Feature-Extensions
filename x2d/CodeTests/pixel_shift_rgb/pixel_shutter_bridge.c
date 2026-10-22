/* 固定官方 X2D 4.2.0：KeyboardInput 的 Event 信号槽；不修改指令与半按。 */
#include <stdint.h>
#include <stddef.h>
extern int snprintf(char*,size_t,const char*,...),sscanf(const char*,const char*,...);
extern void *memcpy(void*,const void*,size_t);
extern int memcmp(const void*,const void*,size_t),strcmp(const char*,const char*),strncmp(const char*,const char*,size_t);
extern size_t strlen(const char*),strcspn(const char*,const char*);
extern char *strcpy(char*,const char*);
extern char *strstr(const char*,const char*);
#define PO_EXTERNAL_C_RUNTIME
#include "pixel_overclock_core.h"
#include "pixel_shutter_policy.h"
#include "pixel_shutter_readiness.h"
#include "pixel_shutter_intent.h"
#include "shutter_fingerprint.h"
typedef uintptr_t U;
extern int open(const char*,int,...),close(int),rename(const char*,const char*),getuid(void);
extern long read(int,void*,U),write(int,const void*,U),readlink(const char*,char*,U),syscall(long,...);
extern int mprotect(void*,U,int),pthread_create(U*,const void*,void*(*)(void*),void*),pthread_detach(U);
extern int posix_spawn(int*,const char*,const void*,const void*,char*const[],char*const[]);
extern char **environ;
struct TS {long s,ns;};
extern int clock_gettime(int,struct TS*),nanosleep(const struct TS*,struct TS*);
struct DlInfo {U addr;const char*name;const void*phdr;unsigned short phnum;};
extern int dl_iterate_phdr(int(*)(struct DlInfo*,U,void*),void*);
#define CHANNEL "/tmp/x2d-pixel-overclock/"
static U base;
static void (*original)(void*,void*);
static unsigned char gate;
static unsigned armed,available,pending,queueable;
static uint64_t canonical_generation[3],queued_generation[3];
static unsigned wait_press;
static uint64_t last_poll;
static char canonical[128],queued[128];
static PoShutterPolicy policy;
static uint64_t now(void){struct TS t;if(clock_gettime(1,&t))return 0;return (uint64_t)t.s*1000+(unsigned long)t.ns/1000000;}
static int trylock(void){return !__atomic_test_and_set(&gate,__ATOMIC_ACQUIRE);}
static void unlock(void){__atomic_clear(&gate,__ATOMIC_RELEASE);}
/* kernel AArch64 stat ABI；不使用宿主 glibc 的 stat 或 pthread_mutex 布局。 */
static int read_regular_stamp(const char *path,char *out,U cap,uint64_t generation[3]){
 int fd=open(path,0xA0800);if(fd<0)return -1;
 uint64_t st[16];int ok=syscall(80,fd,st)==0;
 unsigned mode=0,uid=1;int64_t size=-1;
 if(ok){memcpy(&mode,(char*)st+16,4);memcpy(&uid,(char*)st+24,4);memcpy(&size,(char*)st+48,8);}
 if(!ok||(mode&0170000)!=0100000||uid!=0||size<0||(uint64_t)size>=cap){close(fd);return -1;}
 long n=read(fd,out,cap-1);close(fd);if(n!=size)return -1;out[n]=0;
 if(generation){generation[0]=st[1];generation[1]=st[11];generation[2]=st[12];}
 return (int)n;
}
static int read_regular(const char *path,char *out,U cap){return read_regular_stamp(path,out,cap,0);}
static int put(const char *temp,const char *path,const char *text){
 int fd=open(temp,0xA0241,0600);if(fd<0)return 0;U n=strlen(text);int ok=write(fd,text,n)==(long)n;
 close(fd);return ok&&!rename(temp,path);
}
static void handle(void *self,void *event){
 unsigned key=0;memcpy(&key,(char*)event+0x1c,4);
 if(key!=0x45){original(self,event);return;}
 unsigned down=*((unsigned char*)event+0x20)!=0;
 struct TS pause={0,1000000};while(!trylock())nanosleep(&pause,0);
 int can=available&&!pending&&now()-last_poll<500;
 int result=po_shutter_event(&policy,key,down,__atomic_load_n(&armed,__ATOMIC_ACQUIRE),can);
 if((result==2||result==3)&&queueable&&!pending&&now()-last_poll<500){
  memcpy(queued,canonical,sizeof queued);memcpy(queued_generation,canonical_generation,sizeof queued_generation);
  pending=1;available=0;
 }
 if(result==3)__atomic_add_fetch(&wait_press,1,__ATOMIC_RELAXED);
 unlock();
 if(!result)original(self,event);
}
static int image(struct DlInfo *d,U n,void*x){(void)n;(void)x;
 if(!d->name||!d->name[0]||!strcmp(d->name,"/system/bin/camera-service"))base=d->addr;return 0;
}
static void start_worker(void){
 /* 无自动拍摄。现有 worker.lock 拒绝重复调度；原样保留其它环境，子进程去掉 preload。 */
 char *env[256];unsigned n=0;
 for(unsigned i=0;environ[i];i++){if(n>=254)return;if(strncmp(environ[i],"LD_PRELOAD=",11))env[n++]=environ[i];}
 env[n]=0;char *argv[]={"/system/bin/x2d-pixel-overclock/worker",0};int pid=0;
 posix_spawn(&pid,argv[0],0,0,argv,env);
}
static void *publish(void *unused){
 (void)unused;start_worker();
 char session[9]={0},previous[9]={0},status[1024],arm[128],busy[6],ready[6];
 unsigned ack=0,beat=0,last_beat=0;uint64_t freshness=0;char consumed[128]={0};unsigned error=0;
 unsigned seen_wait_token=0;
 uint64_t consumed_generation[3]={0};
 for(;;){
  struct TS delay={0,50000000};nanosleep(&delay,0);
  int n=read_regular(CHANNEL "status.json",status,sizeof status);
  int valid=n>0&&sscanf(status,"{\"protocol\":3,\"session\":\"%8[0-9a-f]\",\"ack\":%u,\"busy\":%5[a-z],\"ready\":%5[a-z],\"heartbeat\":%u",session,&ack,busy,ready,&beat)==5&&strlen(session)==8;
  uint64_t t=now();
  if(valid&&strcmp(session,previous))seen_wait_token=0;
  if(valid&&(strcmp(session,previous)||beat!=last_beat)){freshness=t;strcpy(previous,session);last_beat=beat;}
  uint64_t generation[3]={0};n=read_regular_stamp(CHANNEL "arm.txt",arm,sizeof arm,generation);PoRequest r;
  int disarmed=n==9&&!strcmp(arm,"DISARMED\n");
  if(disarmed){consumed[0]=0;error=0;}
  if(n>0&&!memcmp(arm,consumed,(U)n)&&memcmp(generation,consumed_generation,sizeof generation)){consumed[0]=0;error=0;}
  int intent=n>0&&!disarmed;
  int request_valid=intent&&valid&&po_parse_request(arm,(U)n,session,ack,&r);
  while(!trylock())nanosleep(&delay,0);
  const char *wait_field=valid?strstr(status,",\"wait_token\":"):0;unsigned wait_token=0;
  if(wait_field&&sscanf(wait_field,",\"wait_token\":%u",&wait_token)==1&&wait_token!=seen_wait_token){
   seen_wait_token=wait_token;
   __atomic_add_fetch(&wait_press,1,__ATOMIC_RELAXED);
  }
  __atomic_store_n(&armed,intent,__ATOMIC_RELEASE);last_poll=t;
  queueable=request_valid&&!strcmp(busy,"false")&&t-freshness<3000&&strcmp(arm,consumed);
  available=queueable&&!strcmp(ready,"true");
#ifdef PO_RAW_STACK
  available=available&&po_shutter_prepared(status,arm);
#else
  (void)po_shutter_prepared;
#endif
  if(request_valid){memcpy(canonical,arm,sizeof canonical);memcpy(canonical_generation,generation,sizeof generation);}else canonical[0]=0;
  if(pending){
   /* 先提交绑定本次模式应用的意图，由 worker 等准备完成后执行一次。 */
   if(queueable&&!strcmp(queued,arm)&&!memcmp(queued_generation,generation,sizeof generation)){
    char intent[256];
    error=!po_intent_write(intent,sizeof intent,queued,generation)||!put(CHANNEL "request.shutter.tmp",CHANNEL "request.txt",intent);
    strcpy(consumed,queued);memcpy(consumed_generation,generation,sizeof generation);available=0;
   }
   pending=0;queued[0]=0;
  }
  char response[320];char echo[128]={0};
  if(request_valid){strcpy(echo,arm);echo[strcspn(echo,"\n")]=0;}
  int waiting=request_valid&&!strcmp(busy,"false")&&!strcmp(arm,consumed)&&!error;
  snprintf(response,sizeof response,"{\"protocol\":1,\"ready\":true,\"arm\":\"%s\",\"available\":%s,\"error\":%u,\"wait_press\":%u,\"pending\":%s}\n",echo,available?"true":"false",error,__atomic_load_n(&wait_press,__ATOMIC_RELAXED),waiting?"true":"false");
  unlock();put(CHANNEL "shutter-status.tmp",CHANNEL "shutter-status.json",response);
 }
 return 0;
}
#ifdef PO_RENDER_ROUTE
#include "pixel_render_hook.inc"
#endif
__attribute__((constructor)) static void init(void){
 char exe[128];long n=readlink("/proc/self/exe",exe,sizeof exe-1);if(n<=0)return;exe[n]=0;
 if(strcmp(exe,"/system/bin/camera-service")||getuid()!=0)return;
#ifdef PO_RENDER_ROUTE
 put("/tmp/x2d-pixel-render-route.tmp","/tmp/x2d-pixel-render-route.status","UNAVAILABLE\n");
#endif
 dl_iterate_phdr(image,0);
 for(unsigned i=0;i<sizeof shutter_checks/sizeof shutter_checks[0];i++)
  if(memcmp((void*)(base+shutter_checks[i].offset),shutter_checks[i].bytes,shutter_checks[i].length))return;
#ifdef PO_RENDER_ROUTE
 /* main 尚未运行；只替换固定指纹的虚函数槽，任务期间不改页权限。 */
 int routed=route_install();
 put("/tmp/x2d-pixel-render-route.tmp","/tmp/x2d-pixel-render-route.status",routed?"READY\n":"UNAVAILABLE\n");
#endif
 U *slot=(U*)(base+0x7a1e00),expected=base+0xb6dd8;
 if(*slot!=expected)return;original=(void(*)(void*,void*))expected;
 U page=(U)slot&~(U)4095;
 if(mprotect((void*)page,4096,3))return;
 __atomic_store_n(slot,(U)handle,__ATOMIC_RELEASE);
 if(mprotect((void*)page,4096,1)){
  __atomic_store_n(slot,expected,__ATOMIC_RELEASE);mprotect((void*)page,4096,1);return;
 }
 U thread;if(pthread_create(&thread,0,publish,0)){
  if(!mprotect((void*)page,4096,3)){__atomic_store_n(slot,expected,__ATOMIC_RELEASE);mprotect((void*)page,4096,1);}return;
 }
 pthread_detach(thread);
}
