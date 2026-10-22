#include "quarter_single_core.h"
static int disjoint(const void *a,size_t na,const void *b,size_t nb){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&na<=UINTPTR_MAX-x&&nb<=UINTPTR_MAX-y&&(x+na<=y||y+nb<=x);
}
/* 输出每个2x2 CFA单元取自输入4x4单元，两个绿色相位分别合并。
 * 全部检查在行块入口；计算循环没有逐像素有效性校验。 */
int quarter_raw_rows(const uint16_t *src,size_t bytes,unsigned sw,unsigned sh,unsigned ss,
 unsigned cx,unsigned cy,unsigned w,unsigned h,uint16_t *dst,size_t capacity,
 unsigned stride,unsigned first,unsigned rows){
 uint64_t sn=(uint64_t)ss*sh*2,dn=(uint64_t)stride*h*2;
 if(!w||!h||(w&1)||(cx&1)||(cy&1)||ss<sw||stride<w||
    cx>sw||cy>sh||2ull*w>sw-cx||2ull*h>sh-cy||
    sn>SIZE_MAX||dn>SIZE_MAX||bytes<sn||capacity<dn||
    !rows||first>=h||rows>h-first||((uintptr_t)src&1)||((uintptr_t)dst&1)||
    !disjoint(src,(size_t)sn,dst,(size_t)dn))return 0;
 for(unsigned y=first;y<first+rows;y++){
  unsigned sy=cy+4*(y/2)+(y&1);
  const uint16_t *a=src+(size_t)sy*ss+cx;
  /* 奇数输出高度的末行保持同色相位，边界向内复制，不读取有效区之外。 */
  const uint16_t *b=sy+2<cy+2*h?a+(size_t)2*ss:a;
  uint16_t *out=dst+(size_t)y*stride;
  for(unsigned x=0;x<w;x++){
   unsigned sx=4*(x/2)+(x&1);
   out[x]=(uint16_t)(((uint32_t)a[sx]+a[sx+2]+b[sx]+b[sx+2]+2)/4);
  }
  for(unsigned x=w;x<stride;x++)out[x]=0;
 }
 return 1;
}
int quarter_nv16_rows(const uint8_t *src,size_t bytes,unsigned sw,unsigned sh,unsigned ss,
 uint8_t *dst,size_t capacity,unsigned w,unsigned h,unsigned stride,unsigned stored_height,unsigned first,unsigned rows){
 uint64_t sp=(uint64_t)ss*sh,dp=(uint64_t)stride*stored_height;
 if(!w||!h||(w&1)||(sw&1)||ss<sw||stride<w||2ull*w>sw||2ull*h>sh||
    stored_height<h||sp>SIZE_MAX/2||dp>SIZE_MAX/2||bytes<sp*2||capacity<dp*2||
    !rows||first>=h||rows>h-first||!disjoint(src,(size_t)(sp*2),dst,(size_t)(dp*2)))return 0;
 for(unsigned y=first;y<first+rows;y++){
  const uint8_t *a=src+(size_t)(2*y)*ss,*b=a+ss;
  const uint8_t *u=a+sp,*v=u+ss;
  uint8_t *out=dst+(size_t)y*stride,*uv=out+dp;
  for(unsigned x=0;x<w;x++)out[x]=(uint8_t)(((unsigned)a[2*x]+a[2*x+1]+b[2*x]+b[2*x+1]+2)/4);
  for(unsigned x=0;x<w;x+=2){
   unsigned sx=2*x;
   uv[x]=(uint8_t)(((unsigned)u[sx]+u[sx+2]+v[sx]+v[sx+2]+2)/4);
   uv[x+1]=(uint8_t)(((unsigned)u[sx+1]+u[sx+3]+v[sx+1]+v[sx+3]+2)/4);
  }
  for(unsigned x=w;x<stride;x++)out[x]=uv[x]=0;
 }
 return 1;
}
