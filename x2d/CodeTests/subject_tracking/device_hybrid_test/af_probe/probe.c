/* 启动期加载，限定 camera-service/官方 4.2.0。不得热注入。 */
#include "probe_core.h"
#include "hooks.h"
#include "lens_speed_allowlist.h"
typedef uintptr_t U;
extern int open(const char*,int,...),close(int),rename(const char*,const char*),getpid(void);
extern long write(int,const void*,U),readlink(const char*,char*,U);
extern char*getenv(const char*);
extern int snprintf(char*,U,const char*,...);
extern int mprotect(void*,U,int),setpriority(int,int,int);
extern int pthread_create(U*,const void*,void*(*)(void*),void*),pthread_detach(U);
struct TS { long s,ns; };
extern int clock_gettime(int,struct TS*),nanosleep(const struct TS*,struct TS*);
struct DlInfo {U addr;const char*name;const void*phdr;unsigned short phnum;};
extern int dl_iterate_phdr(int(*)(struct DlInfo*,U,void*),void*);
extern U af_literals[];
extern int af_speed_clone(int,unsigned,uint16_t*,float);
#ifdef X2D_AFC_TYPE1_3
extern int af_speed_afc_clone(int,unsigned,uint16_t*,float);
static int afc_speed_mode(unsigned);
#ifdef X2D_AF_SPEED_16_24
#define AF_SPEED_LABEL "1.6/2/2.8;AFC-type1=2.4"
#else
#define AF_SPEED_LABEL "3/2/2.8;AFC-type1=3.0"
#endif
#else
#define AF_SPEED_LABEL "3/2/2.8"
#endif
static U base,rcam_base,originals[AF_HOOK_COUNT];
static atomic_int ready;
static int equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int library(struct DlInfo*d,U n,void*x){(void)n;(void)x;
 if(!d->name)return 0;
 const char*name=d->name;for(const char*p=name;*p;p++)if(*p=='/')name=p+1;
 if(equal(name,"libaaa.so"))base=d->addr;
 if(equal(name,"librcam.so"))rcam_base=d->addr;
 return 0;}
static uint64_t now(void){struct TS t;if(clock_gettime(1,&t))return 0;return (uint64_t)t.s*1000000+t.ns/1000;}
static U relative(U a){return a>=base+0x1000&&a<base+0x1ca59c?a-base:0;}
static void speed_type_record(U,unsigned,unsigned,int);
/* 逐次核对镜头与后续加速界限，换镜头自动使用原厂函数。 */
static int speed_allowed(void){
 U lens=*(U*)(base+0xa2f2f0),p=*(U*)(base+0xa2fa00);
 if(!lens||!p||!x2d_speed_lens_allowed(*(unsigned*)(lens+0x30)))return 0;
 const unsigned offs[3]={0x9a0,0x9d0,0xa00};
 for(int i=0;i<3;i++){double f=*(double*)(p+offs[i]);if(!(f>=0&&f<=1.5))return 0;}
 return 1;
}
static int speed(int c,unsigned type,uint16_t*out,float fps){
 U caller=(U)__builtin_return_address(0);unsigned t=type&65535;
 int patch=t<=2&&speed_allowed(),phase=-1;
 if(c==0&&relative(caller)==0x56b38){U state=*(U*)(base+0xa2b0f0);if(state)phase=*(int*)(state+0x20);}
 int r;
#ifdef X2D_AFC_TYPE1_3
 if(patch&&c==0&&t==1&&afc_speed_mode(c))r=af_speed_afc_clone(c,type,out,fps);
 else
#endif
 r=patch?af_speed_clone(c,type,out,fps):((int(*)(int,unsigned,uint16_t*,float))originals[8])(c,type,out,fps);
 speed_type_record(relative(caller),type,(r==0&&out)?*out:0,r);
 af_record((AfEvent){.us=now(),.caller=relative(caller),.kind=AF_SPEED,.context=c,
  .a=(int)type,.b=(r==0&&out)?*out:-1,.result=r,.flags=patch,.phase=phase});
 return r;
}
#include "send_probe.h"
#include "near_runtime.h"
#ifdef X2D_AFC_TYPE1_3
static int afc_speed_mode(unsigned c){
 return !((int(*)(unsigned))(base+NEAR_RECORDING))(c)&&
        ((int(*)(unsigned))(base+NEAR_AFC))(c);
}
#endif
#ifdef X2D_AFC_NEAR_HOLD
#define AF_NEAR_LABEL "near-hold-afc-v1"
#else
#define AF_NEAR_LABEL "near-hold-v1"
#endif
__attribute__((visibility("hidden"))) U af_enter(unsigned i,const U *r){
 if(!atomic_load_explicit(&ready,memory_order_relaxed))return originals[i];
 if(i==8)return (U)speed;
 if(i==14)return (U)container_push;
 if(i==15)return (U)container_pop;
 if(i==16)return (U)set_scan;
 if(i==17)return (U)set_abs;
 if(i==18)return (U)set_command;
 if(i==19)return (U)cdaf_speed;
 if(i==20)return (U)near_set_distance;
 if(i==21)return (U)near_select_speed;
 if(i==22)return (U)near_reset_assistant;
 if((i==0||i==5||i==7)&&r[0]==0)near_reset(i==0);
 AfEvent e={.us=now(),.caller=relative(r[9]),.kind=specs[i].kind,.context=(int)r[0],.a=specs[i].subtype};
 if(e.kind==AF_SCAN_QUEUE){e.context=-1;e.a=(int)r[1];e.b=(int)(r[2]&65535);}
 if(e.kind==AF_POSITION_QUEUE){e.context=-1;e.a=(int)r[1];}
 if(e.kind==AF_STOP_QUEUE)e.context=-1;
 if(e.kind==AF_SCAN_DRIVER){e.a=r[1]?*(int*)r[1]:-1;e.b=r[2]?*(int*)r[2]:-1;}
 if(e.kind==AF_POSITION_DRIVER){e.a=r[1]?*(int*)r[1]:-1;e.b=(int)r[2];}
 if(i==0)atomic_fetch_add(&request_serial,1);
 af_record(e);
 if(i==9)return (U)scan_produce;
 if(i==10)return (U)abs_produce;
 if(i==11)return (U)stop_produce;
 return originals[i];
}
/* 文件与格式化仅在低优先级后台；相机线程遇竞争直接丢记录。 */
static void*publish(void*unused){
 (void)unused;setpriority(0,0,15);
 AfSnapshot s;char text[40000];unsigned prevSession=~0U,prevCount=~0U,prevDrop=~0U;
 for(;;){
  struct TS delay={0,250000000};nanosleep(&delay,0);
  if(!atomic_load_explicit(&ready,memory_order_acquire)||!af_snapshot(&s))continue;
  if(s.session==prevSession&&s.count==prevCount&&s.dropped==prevDrop)continue;
  int n=snprintf(text,sizeof(text),"{\"protocol\":1,\"ready\":true,\"sender\":true,\"speedProfile\":\"" AF_SPEED_LABEL "\",\"nearGuard\":\"" AF_NEAR_LABEL "\",\"guardContention\":%u,\"pid\":%d,\"session\":%u,\"request\":%u,\"originLost\":%u,\"dropped\":%u,\"events\":[",atomic_load(&near_contention),getpid(),s.session,atomic_load(&request_serial),atomic_load(&origins.lost),s.dropped);
  for(unsigned i=0;i<s.count;i++){
   AfEvent*e=&s.events[i];
   n+=snprintf(text+n,sizeof(text)-(U)n,"%s[%llu,%d,%d,%d,%d,%d,%d,%llu,%d,%u,%u,%u]",i?",":"",
    (unsigned long long)e->us,e->kind,e->context,e->a,e->b,e->result,e->flags,(unsigned long long)e->caller,e->phase,e->request,e->send_order,e->speed_type_plus1);
  }
  n+=snprintf(text+n,sizeof(text)-(U)n,"]}\n");
  if(n<=0||(U)n>=sizeof(text))continue;
  int f=open("/tmp/x2d-af-probe.next",0xA0241,0644);
  if(f<0)continue;
  long w=write(f,text,(U)n);close(f);
  if(w==n&&rename("/tmp/x2d-af-probe.next","/tmp/x2d-af-probe.json")==0){prevSession=s.session;prevCount=s.count;prevDrop=s.dropped;}
 }
 return 0;
}
static int attach(void){
 for(unsigned i=0;i<sizeof(near_checks)/sizeof(near_checks[0]);i++)
  if(!same_bytes((void*)(base+near_checks[i].offset),near_checks[i].bytes,near_checks[i].length))return 12;
 for(U k=0;k<sizeof(original_speed_bytes);k++)if(*(unsigned char*)(base+0xac380+k)!=original_speed_bytes[k])return 2;
 for(unsigned i=0;i<AF_HOOK_COUNT;i++){
  originals[i]=base+specs[i].offset;
  if(*(unsigned*)originals[i]!=specs[i].word||*(U*)(base+specs[i].slot)!=originals[i])return 3;
 }
 for(unsigned i=0;i<AF_LITERAL_COUNT;i++)af_literals[i]+=base;
 /* 四页全部预先成功后才原子换槽位；没有修改相机指令、没有暂停工作线程。 */
 U p=base+0x1dc000;unsigned changed=0;
 for(;changed<4;changed++)if(mprotect((void*)(p+changed*4096),4096,3))break;
 if(changed!=4){for(unsigned j=0;j<changed;j++)mprotect((void*)(p+j*4096),4096,1);return 4;}
 for(unsigned i=0;i<AF_HOOK_COUNT;i++)__atomic_store_n((U*)(base+specs[i].slot),(U)entries[i],__ATOMIC_RELEASE);
 int ok=1;for(unsigned j=0;j<4;j++)if(mprotect((void*)(p+j*4096),4096,1))ok=0;
 if(!ok){
  if(!mprotect((void*)p,4*4096,3)){
   for(unsigned i=0;i<AF_HOOK_COUNT;i++)__atomic_store_n((U*)(base+specs[i].slot),originals[i],__ATOMIC_RELEASE);
   mprotect((void*)p,4*4096,1);
  }
  return 5;
 }
 int sender=attach_sender();
 if(sender){
  if(!mprotect((void*)p,4*4096,3)){
   for(unsigned i=0;i<AF_HOOK_COUNT;i++)__atomic_store_n((U*)(base+specs[i].slot),originals[i],__ATOMIC_RELEASE);
   mprotect((void*)p,4*4096,1);
  }
  return sender;
 }
 atomic_store_explicit(&ready,1,memory_order_release);
 return 0;
}
static void status(int reason){
 char text[160];int n=snprintf(text,sizeof(text),"{\"protocol\":1,\"ready\":false,\"pid\":%d,\"reason\":%d,\"events\":[]}\n",getpid(),reason);
 int f=open("/tmp/x2d-af-probe.json",0xA0241,0644);if(f>=0){write(f,text,(U)n);close(f);}
}
static void*startup(void*unused){
 (void)unused;setpriority(0,0,15);status(1);
 /* 原厂可能后续 dlopen 对焦库。有限等待只发生在我们的后台线程。 */
 for(unsigned i=0;i<120&&(!base||!rcam_base);i++){
  dl_iterate_phdr(library,0);
  if(!base||!rcam_base){struct TS delay={0,250000000};nanosleep(&delay,0);}
 }
 if(!base||!rcam_base){status(6);return 0;}
 int reason=attach();if(reason){status(reason);return 0;}
 return publish(0);
}
__attribute__((constructor))static void install(void){
 char path[128];char*on=getenv("X2D_AF_PROBE");if(!on||!equal(on,"1"))return;
 long n=readlink("/proc/self/exe",path,sizeof(path)-1);if(n<=0||n>=(long)sizeof(path)-1)return;
 path[n]=0;if(!equal(path,"/system/bin/camera-service"))return;
 U worker;if(pthread_create(&worker,0,startup,0)){status(7);return;}pthread_detach(worker);
}
