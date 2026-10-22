/* X2D 4.2.0：只提前结束 AFC 的画面稳定等待，不改变原厂场景变化检测。 */
#include <stdint.h>
#include "afc_settle_policy.h"
typedef uintptr_t U;
extern int open(const char*,int,...),close(int),getpid(void),mprotect(void*,U,int);
extern long write(int,const void*,U),readlink(const char*,char*,U);
extern char *getenv(const char*);
extern int snprintf(char*,U,const char*,...);
extern int pthread_create(U*,const void*,void*(*)(void*),void*),pthread_detach(U);
struct TS {long seconds,nanoseconds;};
extern int nanosleep(const struct TS*,struct TS*);
#include "afc_startup_order.h"
struct DlInfo { U addr; const char *name; const void *phdr; unsigned short phnum; };
extern int dl_iterate_phdr(int(*)(struct DlInfo*,U,void*),void*);
static U base;
#include "settle_layout.h"
static int equal(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return *a==*b;}
static int library(struct DlInfo *d,U n,void *x){(void)n;(void)x;
    if(!d->name)return 0;const char *p=d->name;
    for(const char *q=p;*q;++q)if(*q=='/')p=q+1;
    if(equal(p,"libaaa.so")){base=d->addr;return 1;}return 0;
}
__attribute__((noinline)) static int quick(int channel){
    int original=((int(*)(int))(base+QUICK_OFFSET))(channel);
    if(original!=1 || channel!=0)return original;
    int afc=((int(*)(int))(base+MODE_OFFSET))(channel);
    if(afc!=1)return original;
    U common=((U(*)(int))(base+COMMON_OFFSET))(channel);
    if(!common)return original;
    unsigned stable=*(unsigned*)(common+16);
    unsigned fps100=((unsigned(*)(int))(base+FPS_OFFSET))(channel);
    return afc_settle_result(original,(unsigned)channel,afc,stable,fps100);
}
__attribute__((noinline)) static int attach(void){
    for(unsigned i=0;i<CHECK_COUNT;++i)
        for(unsigned j=0;j<checks[i].size;++j)
            if(((unsigned char*)(base+checks[i].offset))[j]!=checks[i].bytes[j])return 2;
    U *slot=(U*)(base+QUICK_SLOT),page=(U)slot&~(U)4095;
    if(*slot!=base+QUICK_OFFSET)return 3;
    if(mprotect((void*)page,4096,3))return 4;
    __atomic_store_n(slot,(U)quick,__ATOMIC_RELEASE);
    if(mprotect((void*)page,4096,1)){
        /* 原页仍可写时撤回槽位；只读恢复失败仍明确报告失败。 */
        __atomic_store_n(slot,base+QUICK_OFFSET,__ATOMIC_RELEASE);
        return mprotect((void*)page,4096,1)?6:5;
    }
    return 0;
}
static void state(int r){
    char text[220];int len=snprintf(text,sizeof(text),
#ifdef X2D_AFC_SETTLE_ONE_FRAME
        "{\"protocol\":1,\"ready\":%s,\"reason\":%d,\"pid\":%d,\"settleFrames\":1,\"nativeParametersModified\":false}\n",
#else
        "{\"protocol\":1,\"ready\":%s,\"reason\":%d,\"pid\":%d,\"settleUs\":33333,\"nativeParametersModified\":false}\n",
#endif
        r?"false":"true",r,getpid());
    int f=open("/tmp/x2d-afc-settle.json",0xA0241,0644);
    if(f>=0){write(f,text,(U)len);close(f);}
}
__attribute__((noinline)) static void* startup(void*unused){
    (void)unused;
    for(unsigned i=0;i<120&&!base;++i){
        dl_iterate_phdr(library,0);
        if(!base){struct TS delay={0,250000000};nanosleep(&delay,0);}
    }
    state(!base?1:!afc_wait_predecessors(2)?10:attach());
    return 0;
}
__attribute__((constructor)) static void install(void){
    char *on=getenv("X2D_AFC_SETTLE_33MS");if(!on||!equal(on,"1"))return;
    char path[128];long n=readlink("/proc/self/exe",path,sizeof(path)-1);
    if(n<=0||n>=(long)sizeof(path)-1)return;path[n]=0;
    if(!equal(path,"/system/bin/camera-service"))return;
    U thread;
    if(pthread_create(&thread,0,startup,0)){state(7);return;}
    pthread_detach(thread);
}
