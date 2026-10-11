/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "factory_light_sharpen.h"
int main(void){
 enum {W=17,H=13,S=24};uint8_t image[S*H*2],source[sizeof image],scratch[W*3];
 memset(image,127,sizeof image);assert(factory_light_sharpen(image,W,H,S,scratch,sizeof scratch));
 for(unsigned i=0;i<sizeof image;i++)assert(image[i]==127);
 for(unsigned i=0;i<sizeof image;i++)image[i]=(uint8_t)((i*31+i/7)%256);
 memcpy(source,image,sizeof image);
 assert(!factory_light_sharpen(image,W,H,S,scratch,sizeof scratch-1));
 assert(!memcmp(image,source,sizeof image));
 assert(factory_light_sharpen(image,W,H,S,scratch,sizeof scratch));
 unsigned changed=0;
 for(unsigned r=0;r<H;r++)for(unsigned x=0;x<S;x++){
  int expected=source[r*S+x];
  if(r&&r+1<H&&x&&x+1<W){
   int sum=0;static const int weight[3]={1,2,1};
   for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)sum+=weight[dy+1]*weight[dx+1]*source[(r+dy)*S+x+dx];
   int d=16*expected-sum,sign=d<0?-1:1;if(d<0)d=-d;
   int step=d>32?(d-32+64)/128:0;if(step>3)step=3;
   expected+=sign*step;if(expected<0)expected=0;if(expected>255)expected=255;
  }
  assert(image[r*S+x]==expected);
  int delta=(int)image[r*S+x]-source[r*S+x];assert(delta>=-3&&delta<=3);changed+=delta!=0;
 }
 assert(changed>0);assert(!memcmp(image+S*H,source+S*H,S*H));
 puts("LIGHT_SHARPEN_REFERENCE_BOUNDARIES_CHROMA_AND_LIMITS_PASSED");return 0;
}
