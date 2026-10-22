/* 流式 SHA-256，只用于中间文件完整内容校验；不输出文件内容或路径。 */
#ifndef X2D_PUBLIC_FINGERPRINT_SHA256_H
#define X2D_PUBLIC_FINGERPRINT_SHA256_H
#include <stdint.h>
#include <string.h>
typedef struct {uint32_t h[8];uint64_t bytes;unsigned used;unsigned char block[64];} ReceiptSha;
static uint32_t rsha_rotr(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
static void rsha_block(ReceiptSha *s,const unsigned char *p){
 static const uint32_t k[64]={
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t w[64];for(unsigned i=0;i<16;i++)w[i]=(uint32_t)p[4*i]<<24|(uint32_t)p[4*i+1]<<16|(uint32_t)p[4*i+2]<<8|p[4*i+3];
 for(unsigned i=16;i<64;i++){
  uint32_t a=w[i-15],b=w[i-2];
  w[i]=w[i-16]+(rsha_rotr(a,7)^rsha_rotr(a,18)^(a>>3))+w[i-7]+(rsha_rotr(b,17)^rsha_rotr(b,19)^(b>>10));
 }
 uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
 for(unsigned i=0;i<64;i++){
  uint32_t t=h+(rsha_rotr(e,6)^rsha_rotr(e,11)^rsha_rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
  uint32_t u=(rsha_rotr(a,2)^rsha_rotr(a,13)^rsha_rotr(a,22))+((a&b)^(a&c)^(b&c));
  h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
 }
 s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void rsha_init(ReceiptSha *s){
 static const uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 memset(s,0,sizeof *s);memcpy(s->h,h,sizeof h);
}
static void rsha_update(ReceiptSha *s,const unsigned char *p,unsigned n){
 s->bytes+=n;
 while(n){unsigned take=64-s->used;if(take>n)take=n;memcpy(s->block+s->used,p,take);s->used+=take;p+=take;n-=take;
  if(s->used==64){rsha_block(s,s->block);s->used=0;}
 }
}
static void rsha_final(ReceiptSha *s,unsigned char out[32]){
 uint64_t bits=s->bytes*8;unsigned char pad[128]={0x80};unsigned n=s->used<56?56-s->used:120-s->used;
 for(unsigned i=0;i<8;i++)pad[n+i]=(unsigned char)(bits>>(56-8*i));
 rsha_update(s,pad,n+8);
 for(unsigned i=0;i<8;i++)for(unsigned j=0;j<4;j++)out[4*i+j]=(unsigned char)(s->h[i]>>(24-8*j));
}
#endif

/* 兼容性检查采用 SHA-256 摘要。 */
static inline int public_code_digest(const void *address,unsigned size,const char *expected){
 if(!address||!expected)return 0;
 unsigned char actual[32];ReceiptSha s;rsha_init(&s);rsha_update(&s,address,size);rsha_final(&s,actual);
 static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<32;i++)if(expected[i*2]!=hex[actual[i]>>4]||expected[i*2+1]!=hex[actual[i]&15])return 0;
 return expected[64]==0;
}

/* X2D 100C 4.2.0 专用启动候选。只修改本进程的 MLControl 虚表；未验证前不得加载到相机服务。 */
#include "face_lock_policy.h"
typedef __UINTPTR_TYPE__ U;
typedef __INT64_TYPE__ I64;
extern int open(const char*,int,...),close(int),mprotect(void*,U,int),getpid(void);
extern long readlink(const char*,char*,U),write(int,const void*,U);
extern char *getenv(const char*);
extern int snprintf(char*,U,const char*,...);
struct DlInfo { U addr; const char *name; const void *phdr; unsigned short phnum; };
extern int dl_iterate_phdr(int(*)(struct DlInfo*,U,void*),void*);
static FaceLock held={0,-1};
static U base;
static void (*original_set)(void*,int);
static void (*original_clear)(void*);
static int (*original_best)(void*);
static int (*original_intersect)(void*,const void*);
static void (*original_tracking)(void*,int);
static int equal(const char*a,const char*b) { while(*a&&*a==*b){a++;b++;}return *a==*b; }
#ifdef FACE_EYE_BACKEND
#ifndef FACE_BACKEND_TEST
static void eye_install_age_limit(void);
static void status(int);
#endif
#include "eye_af_bridge.h"
#include "eye_age_limit.h"
#endif
#ifndef FACE_BACKEND_TEST
static int main_base(struct DlInfo*d,U n,void*x) { (void)n;(void)x;if(!d->name||!d->name[0]||equal(d->name,"/system/bin/camera-service")){base=d->addr;return 1;}return 0; }
#endif
static int target_present(void *self) {
    if(!face_locked(&held,self))return 0;
    /* 原厂 bestFaceTrackingId 的同一 getter 与 QList<FaceInfo> 布局。 */
    U *table=*(U**)self;
    const unsigned char *list=((const unsigned char*(*)(void*))table[0x98/8])(self);
    if(!list)return 0;
    I64 count=*(const I64*)(list+16);
    const unsigned char *data=*(const unsigned char*const*)(list+8);
    if(count<0||count>4096||(!data&&count))return 0;
    for(I64 i=0;i<count;i++)if(*(const int*)(data+i*0x118)==held.id)return 1;
    return 0;
}
static void set_target(void*self,int command) {
#ifdef FACE_EYE_BACKEND
    if(eye_command(command))return;
#endif
    int target;
    int action=face_command(&held,self,command,&target);
    if(action==2) { original_clear(self);return; }
    if(action==3)return; /* 已锁时忽略普通自动选人，不改变用户身份。 */
    original_set(self,target);
}
static void clear_target(void*self) { if(!face_locked(&held,self))original_clear(self); }
static int best_target(void*self) {
    if(target_present(self))return held.id;
    return original_best(self); /* 暂失时可白框临时选人，但不抹去 held。 */
}
static int intersect_target(void*self,const void*rect) {
    if(target_present(self))return held.id;
    return original_intersect(self,rect);
}
static void tracking_target(void*self,int id) {
    original_tracking(self,target_present(self)?held.id:id);
}
#ifndef FACE_BACKEND_TEST
static void status(int ready) {
    char text[144];int n=snprintf(text,sizeof(text),"{\"protocol\":1,\"ready\":%s,\"pid\":%d}\n",ready?"true":"false",getpid());
#ifdef FACE_EYE_BACKEND
    n=snprintf(text,sizeof(text),"{\"protocol\":1,\"eyeProtocol\":2,\"ageFrames\":%d,\"ageError\":%d,\"ready\":%s,\"pid\":%d}\n",eye_age_frames,eye_age_error,ready?"true":"false",getpid());
#endif
    int f=open("/tmp/x2d-face-lock-backend.json",0xA0241,0644);
    if(f>=0){write(f,text,(U)n);close(f);}
}
__attribute__((constructor))static void install(void) {
    char path[128];char*enabled=getenv("X2D_FACE_LOCK_BACKEND");
    if(!enabled||!equal(enabled,"1"))return;
    long n=readlink("/proc/self/exe",path,sizeof(path)-1);
    if(n<=0||n>=(long)sizeof(path)-1)return;
    path[n]=0;if(!equal(path,"/system/bin/camera-service"))return;
    status(0);dl_iterate_phdr(main_base,0);if(!base)return;
    U *table=(U*)(base+0x7a20d8+16);
    const U slots[5]={0xa8/8,0xb0/8,0xb8/8,0xe8/8,0x118/8};
    const U offsets[5]={0x223c18,0x223bc0,0x2233d8,0x223ab0,0x223388};
    const char *words[5]={"c184cb1ec16cdbaa2fce888b878c2c194dd4ecc469f8a3519f551e93d8e5683f","7b2f00e1f4e4653d6f42ec87b4f454337a60710be3dde2a4c6d329874e2c81c6","63b368cdb6ab9ee53a7cd531602553504c8573b654a5d4e620add844a006e9f4","7cabd3739cabc225c6ea1bd0347805cfdbf7e8969bada92c2eae548fb070eb9a","3f07c89f3e39893d4731c5865d01df58e36cb6a0787414cf6d4da7d46a6f08b8"};
    for(int i=0;i<5;i++)if(table[slots[i]]!=base+offsets[i]||!public_code_digest((void*)(base+offsets[i]),4,words[i]))return;
    original_best=(void*)(base+offsets[0]);original_intersect=(void*)(base+offsets[1]);
    original_set=(void*)(base+offsets[2]);original_clear=(void*)(base+offsets[3]);
    original_tracking=(void*)(base+offsets[4]);
    U page=(U)table&~4095UL;
    if(mprotect((void*)page,4096,3))return;
    table[slots[0]]=(U)best_target;table[slots[1]]=(U)intersect_target;
    table[slots[2]]=(U)set_target;table[slots[3]]=(U)clear_target;
    table[slots[4]]=(U)tracking_target;
    if(mprotect((void*)page,4096,1)) {
        for(int i=0;i<5;i++)table[slots[i]]=base+offsets[i];
        mprotect((void*)page,4096,1);return;
    }
#ifdef FACE_EYE_BACKEND
    eye_install_age_limit();
#endif
    status(1);
}
#endif
