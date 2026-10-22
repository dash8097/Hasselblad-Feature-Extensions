/* 流式 SHA-256，只用于中间文件完整内容校验；不输出文件内容或路径。 */
#ifndef RECEIPT_SHA256_H
#define RECEIPT_SHA256_H
#include <stdint.h>
#include <string.h>
#ifdef __aarch64__
#include <arm_neon.h>
#include <sys/auxv.h>
__attribute__((target("sha2")))
static void rsha_rounds_hw(uint32_t h[8],const uint32_t w[64],const uint32_t k[64]){
 uint32x4_t a=vld1q_u32(h),b=vld1q_u32(h+4),original_a=a,original_b=b;
 for(unsigned i=0;i<64;i+=4){
  uint32x4_t values=vaddq_u32(vld1q_u32(w+i),vld1q_u32(k+i));
  uint32x4_t next=vsha256hq_u32(a,b,values);
  b=vsha256h2q_u32(b,a,values);a=next;
 }
 vst1q_u32(h,vaddq_u32(original_a,a));vst1q_u32(h+4,vaddq_u32(original_b,b));
}
#endif
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
#ifdef __aarch64__
 /* Linux HWCAP_SHA2；当前硬件也已只读确认支持，仍保留运行时回退。 */
 if(getauxval(16)&(1UL<<6)){rsha_rounds_hw(s->h,w,k);return;}
#endif
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
