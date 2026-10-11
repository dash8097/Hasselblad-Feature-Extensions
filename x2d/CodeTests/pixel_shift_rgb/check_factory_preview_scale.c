/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#include <stdio.h>
#include <string.h>
#include "factory_preview_scale.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"preview check failed line %d\n",__LINE__);return 1;}}while(0)
static void reference(const uint8_t *src,unsigned sw,unsigned sh,unsigned ss,
 uint8_t *out,unsigned tw,unsigned th,unsigned ts){
 for(unsigned p=0;p<2;p++)for(unsigned y=0;y<th;y++){
  unsigned c=p?2:1;
  for(unsigned x=0;x<tw/c;x++){
   unsigned l=x*(sw/c)/(tw/c),r=(x+1)*(sw/c)/(tw/c);
   unsigned t=y*sh/th,b=(y+1)*sh/th,n=(r-l)*(b-t);
   for(unsigned k=0;k<c;k++){
    unsigned sum=0;
    for(unsigned yy=t;yy<b;yy++)for(unsigned xx=l;xx<r;xx++)sum+=src[p*ss*sh+yy*ss+xx*c+k];
    out[p*ts*th+y*ts+x*c+k]=(uint8_t)((sum+n/2)/n);
   }
  }
  for(unsigned x=tw;x<ts;x++)out[p*ts*th+y*ts+x]=0;
 }
}
int main(void){
 uint8_t source[2*16*8],original[sizeof source],target[2*8*4+8];
 memset(source,199,sizeof source);
 for(unsigned y=0;y<8;y++)for(unsigned x=0;x<12;x++){
  source[y*16+x]=(uint8_t)(20*y+x);
  source[128+y*16+x]=(uint8_t)((x&1)?180+y:30+x/2);
 }
 memcpy(original,source,sizeof source);memset(target,0xa5,sizeof target);
 CHECK(factory_preview_scale(source,12,8,16,target,4,4,8));
 const uint8_t luma[]={11,14,17,20,51,54,57,60,91,94,97,100,131,134,137,140};
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<8;x++){
  CHECK(target[y*8+x]==(x<4?luma[y*4+x]:0));
  CHECK(target[32+y*8+x]==(x<4?((x&1)?181+2*y:31+(x/2)*3):0));
 }
 CHECK(!memcmp(source,original,sizeof source));
 for(unsigned x=64;x<sizeof target;x++)CHECK(target[x]==0xa5);
 /* 不整除的范围也必须读到最右/最下样本；U、V 不得互混。 */
 memset(source,0,sizeof source);source[5*16+9]=255;
 for(unsigned y=0;y<6;y++)for(unsigned x=0;x<10;x++)source[96+y*16+x]=(x&1)?230:20;
 CHECK(factory_preview_scale(source,10,6,16,target,4,4,8));
 CHECK(target[3*8+3]==43);
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)CHECK(target[32+y*8+x]==((x&1)?230:20));
 CHECK(!factory_preview_scale(source,10,6,16,target,12,4,16));
 CHECK(!factory_preview_scale(source,10,6,16,target,3,4,8));
 CHECK(!factory_preview_scale(source,10,6,16,source,4,4,8));
 uint8_t input[2*72*48],expected[2*24*48],actual[sizeof expected];unsigned seed=9;
 for(unsigned i=0;i<sizeof input;i++){seed=seed*1664525u+1013904223u;input[i]=(uint8_t)(seed>>24);}
 for(unsigned tw=2;tw<=20;tw+=2)for(unsigned th=2;th<=48;th+=3){
  memset(actual,0xa5,sizeof actual);memset(expected,0xa5,sizeof expected);
  reference(input,64,48,72,expected,tw,th,24);
  CHECK(factory_preview_scale(input,64,48,72,actual,tw,th,24));
  CHECK(!memcmp(actual,expected,sizeof actual));
 }
 puts("PREVIEW_AREA_COVERAGE_CHROMA_PADDING_AND_SOURCE_PRESERVATION_PASSED");return 0;
}
