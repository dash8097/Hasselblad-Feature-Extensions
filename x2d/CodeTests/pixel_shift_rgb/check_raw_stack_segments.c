/* 非连续行段与原连续核心逐值对照；源仅由本程序生成。 */
#include "raw_stack_parallel.h"
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static void compare(unsigned method,unsigned noise,unsigned threads){
 RsConfig c={15,139,17,(RsMethod)method,noise};size_t total;assert(rs_workspace_bytes(c,&total));
 void *whole=malloc(total);RsCore reference,segmented;assert(rs_init(&reference,c,whole,total));
 RsSegment parts[8];uint32_t sizes[]={1,7,33,2,49,3,9,35};
 for(unsigned i=0;i<8;i++){
  RsConfig p=c;p.height=sizes[i];size_t bytes;assert(rs_workspace_bytes(p,&bytes));
  parts[i]=(RsSegment){malloc(bytes+16),bytes,p.height};assert(parts[i].workspace);memset(parts[i].workspace,0xa5,bytes+16);
 }
 assert(rs_init_segments(&segmented,c,parts,8));
 uint16_t *source=malloc(reference.samples*2);assert(source);
 for(unsigned frame=0;frame<13;frame++){
  for(size_t i=0;i<reference.samples;i++)source[i]=(uint16_t)(i*8191+frame*21337);
  assert(rs_parallel_frame(&reference,source,reference.samples*2,threads));
  assert(rs_parallel_frame(&segmented,source,reference.samples*2,threads));
 }
 size_t bytes;const uint16_t *expected=rs_parallel_finish(&reference,threads,&bytes);assert(expected);
 RsPixelBlock output[8];size_t actual;assert(rs_parallel_finish_blocks(&segmented,threads,output,8,&actual)==8&&actual==bytes);
 size_t offset=0;
 for(unsigned i=0;i<8;i++){
  assert(!memcmp((const uint8_t*)expected+offset,output[i].pixels,output[i].bytes));offset+=output[i].bytes;
  for(unsigned n=0;n<16;n++)assert(((uint8_t*)parts[i].workspace)[parts[i].bytes+n]==0xa5);
  free(parts[i].workspace);
 }
 assert(!rs_parallel_frame(&segmented,source,actual,threads));free(source);free(whole);
}
static void limits(void){
 _Alignas(8) uint32_t a[8],b[8];RsConfig c={1,2,1,RS_AVERAGE,0};RsCore s;
 RsSegment parts[2]={{a,8,1},{b,8,1}};
 assert(!rs_init_segments(&s,c,parts,1));parts[1].workspace=a;assert(!rs_init_segments(&s,c,parts,2));
 parts[1].workspace=b;parts[1].bytes=3;assert(!rs_init_segments(&s,c,parts,2));parts[1].bytes=8;
 assert(rs_init_segments(&s,c,parts,2));
 /* 用户上限65535帧；分段均值保持精确，拒绝再增加一帧。 */
 s.frames=RS_MAX_FRAMES;a[0]=UINT32_C(65535)*65535;b[0]=UINT32_C(65533)*65535;
 assert(!rs_begin_frame(&s,4));
 RsPixelBlock out[2];size_t bytes;
 assert(!rs_parallel_finish_blocks(&s,4,out,1,&bytes)&&!s.frame_open);
 assert(rs_parallel_finish_blocks(&s,4,out,2,&bytes)==2&&bytes==4);
 assert(out[0].pixels[0]==65535&&out[1].pixels[0]==65533);
 assert(!rs_parallel_finish_blocks(&s,4,out,2,&bytes));
}
void rs_check_segments(void){
 limits();for(unsigned t=1;t<=4;t++)for(unsigned m=0;m<3;m++){
  compare(m,0,t);
 }
 puts("RAW_STACK_NONCONTIGUOUS_EXACT_AVERAGE_EXTREMA_PARALLEL_AND_U32_LIMIT_PASSED");
}
#ifndef RS_SEGMENT_TEST_NO_MAIN
int main(void){rs_check_segments();return 0;}
#endif
