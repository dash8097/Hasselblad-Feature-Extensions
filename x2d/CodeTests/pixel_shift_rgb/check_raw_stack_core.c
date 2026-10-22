#include "raw_stack_core.h"
#include "raw_stack_parallel.h"
#include "raw_stack_session.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void arithmetic(RsMethod method,unsigned noise,unsigned frames,unsigned threads){
 /* 奇数 stride/奇数总数覆盖 group sum 对齐；超过64行覆盖分块复制。 */
 RsConfig c={15,139,17,method,noise};size_t bytes=0,n=(size_t)c.stride_samples*c.height;
 assert(rs_workspace_bytes(c,&bytes));void *mem=malloc(bytes+16);assert(mem);memset(mem,0xa5,bytes+16);
 RsCore s;assert(rs_init(&s,c,mem,bytes));
 uint16_t *src=malloc(n*2),*expected=calloc(n,2);uint64_t *sum=calloc(n,8);uint32_t *group=calloc(n,4);
 assert(src&&expected&&sum&&group);unsigned pending=0,groups=0;
 for(unsigned f=0;f<frames;f++){
  for(size_t i=0;i<n;i++)src[i]=(uint16_t)((i*8191+f*21337+(f%3)*1023)&65535);
  assert(rs_parallel_frame(&s,src,n*2,threads));
  for(size_t i=0;i<n;i++){
   if(method==RS_AVERAGE)sum[i]+=src[i];
   else if(noise)group[i]+=src[i];
   else if(!f||(method==RS_MAXIMUM?src[i]>expected[i]:src[i]<expected[i]))expected[i]=src[i];
  }
  if(noise&&++pending==noise){
   for(size_t i=0;i<n;i++){uint16_t v=(uint16_t)((group[i]+pending/2)/pending);
    if(!groups||(method==RS_MAXIMUM?v>expected[i]:v<expected[i]))expected[i]=v;group[i]=0;}
   groups++;pending=0;
  }
 }
 for(size_t i=0;i<n;i++){
  if(method==RS_AVERAGE)expected[i]=(uint16_t)((sum[i]+frames/2)/frames);
  else if(noise&&pending){uint16_t v=(uint16_t)((group[i]+pending/2)/pending);
   if(!groups||(method==RS_MAXIMUM?v>expected[i]:v<expected[i]))expected[i]=v;}
 }
 size_t output_bytes;const uint16_t *result=rs_parallel_finish(&s,threads,&output_bytes);
 assert(result&&output_bytes==n*2&&s.frames==frames&&!memcmp(expected,result,n*2));
 for(unsigned i=0;i<16;i++)assert(((uint8_t *)mem)[bytes+i]==0xa5);
 assert(!rs_parallel_frame(&s,src,n*2,threads));assert(!rs_parallel_finish(&s,threads,&output_bytes));
 free(src);free(expected);free(sum);free(group);free(mem);
}
static void validation(void){
 size_t bytes;RsConfig c={1,1,1,RS_AVERAGE,0};_Alignas(8) uint32_t mem[16];RsCore s;
 assert(!rs_workspace_bytes((RsConfig){1,1,0,RS_AVERAGE,0},&bytes));
 assert(!rs_workspace_bytes((RsConfig){1,1,1,RS_AVERAGE,4},&bytes));
 assert(!rs_workspace_bytes((RsConfig){1,1,1,(RsMethod)-1,0},&bytes));
 assert(!rs_init(&s,c,(uint8_t *)mem+1,sizeof mem-1));assert(!rs_init(&s,c,mem,3));
 assert(rs_init(&s,c,mem,sizeof mem));assert(!rs_begin_finish(&s));assert(!rs_begin_frame(&s,1));
 assert(rs_begin_frame(&s,2));assert(!rs_begin_frame(&s,2));assert(!rs_begin_finish(&s));
 uint16_t v=65535;assert(!rs_accumulate_block(&s,&v,1,0,1));assert(!rs_accumulate_rows(&s,&v,1,1));
 assert(rs_accumulate_rows(&s,&v,0,1));assert(rs_commit_frame(&s));assert(!rs_commit_frame(&s));
 s.frames=RS_MAX_FRAMES;mem[0]=UINT32_C(65535)*65535;
 assert(!rs_begin_frame(&s,2));
 const uint16_t *result=rs_parallel_finish(&s,1,&bytes);assert(result&&result[0]==65535&&bytes==2);
 c=(RsConfig){11836,8842,11904,RS_AVERAGE,0};assert(rs_workspace_bytes(c,&bytes)&&bytes==421020672);
 c.method=RS_MAXIMUM;assert(rs_workspace_bytes(c,&bytes)&&bytes==210510336);
 for(unsigned m=0;m<3;m++)for(unsigned noise=1;noise<=4;noise++){
  c.method=(RsMethod)m;c.noise_group=noise;assert(!rs_workspace_bytes(c,&bytes));
 }
}
static void session(void){
 RsSession s={0};uint64_t token;
 assert(!rs_session_prepare(&s,0));assert(rs_session_prepare(&s,2));assert(!rs_session_next(&s,100,&token));
 assert(rs_session_start(&s,100));assert(rs_session_next(&s,101,&token)&&token==1);
 assert(!rs_session_next(&s,102,&token));assert(!rs_session_claim(&s,2,3));
 assert(rs_session_claim(&s,1,3));assert(!rs_session_claim(&s,1,3));
 rs_session_finish(&s);assert(!rs_session_output_ready(&s,150));
 assert(!rs_session_frame_done(&s,1,4,160,1));assert(rs_session_frame_done(&s,1,3,160,1));
 assert(!rs_session_frame_done(&s,1,3,160,1));assert(rs_session_output_ready(&s,161));
 assert(!rs_session_complete(&s,1,0,1)&&s.phase==RS_FAILED);
 assert(rs_session_prepare(&s,1));assert(rs_session_start(&s,100));
 assert(rs_session_next(&s,100,&token));assert(rs_session_claim(&s,token,0));
 assert(rs_session_frame_done(&s,token,0,1050,1));assert(rs_session_next(&s,1099,&token));
 assert(rs_session_claim(&s,token,0));assert(rs_session_frame_done(&s,token,0,1500,1));
 assert(s.frames==2&&s.gap_ms==49);assert(rs_session_output_ready(&s,1501));
 assert(rs_session_complete(&s,1,1,1)&&s.phase==RS_COMPLETE);
 assert(rs_session_prepare(&s,1));assert(rs_session_start(&s,100));rs_session_finish(&s);
 assert(!rs_session_output_ready(&s,101)&&s.phase==RS_FAILED);
}
int main(void){
 extern void rs_check_segments(void);rs_check_segments();
 validation();session();unsigned cases=0;
 for(unsigned t=1;t<=4;t++)for(unsigned m=0;m<3;m++)for(unsigned f=1;f<=101;f+=20){
  arithmetic((RsMethod)m,0,f,t);cases++;
 }
 printf("RAW_STACK_CORE_OK cases=%u exact_average_maximum_minimum_fixed_memory timer_and_manual_finish\n",cases);return 0;
}
