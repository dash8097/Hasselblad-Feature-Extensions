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

/* 公开实现仅含兼容性摘要，不保存厂家指令。SHA-256 不是解密。 */
static inline int public_code_digest(const void *address,unsigned size,const char *expected){
 if(!address||!expected)return 0;
 unsigned char actual[32];ReceiptSha s;rsha_init(&s);rsha_update(&s,address,size);rsha_final(&s,actual);
 static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<32;i++)if(expected[i*2]!=hex[actual[i]>>4]||expected[i*2+1]!=hex[actual[i]&15])return 0;
 return expected[64]==0;
}

/* 仅绑定官方 4.2.0 HAL。open 的已启动分支仍增加引用并返回 -1004；
 * close 的多引用分支只减计数，不停止/关闭设备。匹配这两个指纹后
 * 才接受这条原厂引用路径，禁止重初始化、start 或 stop 现有图像池。 */
#ifndef CAPTURE_ION_REFERENCE_H
#define CAPTURE_ION_REFERENCE_H
#include <stdint.h>
#include <stddef.h>
static inline int cir_fingerprint(const void *address,const uint8_t *expected,size_t size){
 if(!address)return 0;
 const uint8_t *actual=address;
 for(size_t i=0;i<size;i++)if(actual[i]!=expected[i])return 0;
 return 1;
}
static inline int cir_borrow(int (*open_device)(const char *,void *,void **),
 int (*close_device)(void *),int (*get_state)(void *,uint8_t *),void **out){
 const char increment[]="78e547b1d03185f787e42aa7d41019f335ef72e1c3b2070fdf7865225a5c66eb";
 const char decrement[]="f2091f94d8d66e599fe100be7783e211b2a22f2e7083d526e9d01206a50b9322";
 if(!out||*out||!open_device||!close_device||!get_state||
    !public_code_digest((const uint8_t*)(uintptr_t)open_device+0x2a4,20,increment)||
    !public_code_digest((const uint8_t*)(uintptr_t)close_device+0x228,12,decrement))return 0;
 int config=0;void *device=NULL;uint8_t state=0;
 int rc=open_device("/dev/ion",&config,&device);
 if(rc!=-1004&&rc!=0)return 0;
 if(!device)return 0;
 if(rc!=-1004||get_state(device,&state)||state!=2){(void)close_device(device);return 0;}
 *out=device;return 1;
}
#endif
