#include "raw_stack_core.h"
#include <string.h>
#include <limits.h>
static int geometry(RsConfig c,size_t *n){
 if(!c.width||!c.height||c.stride_samples<c.width||c.method<RS_MAXIMUM||c.method>RS_AVERAGE||
    (c.method==RS_AVERAGE&&c.noise_group)||c.noise_group>65535)return 0;
 uint64_t count=(uint64_t)c.stride_samples*c.height;
 if(count>SIZE_MAX/8)return 0;
 *n=(size_t)count;return 1;
}
int rs_workspace_bytes(RsConfig c,size_t *bytes){
 size_t n;if(!bytes||!geometry(c,&n))return 0;
 *bytes=c.method==RS_AVERAGE?n*4:c.noise_group?((n*2+7)&~(size_t)7)+n*4:n*2;return 1;
}
int rs_init(RsCore *s,RsConfig c,void *buffer,size_t bytes){
 size_t required,n;
 if(!s||!buffer||((uintptr_t)buffer&7)||!geometry(c,&n)||
    !rs_workspace_bytes(c,&required)||bytes<required)return 0;
 *s=(RsCore){.config=c,.workspace=buffer,.bytes=required,.samples=n};
 /* 初始化由第一帧覆盖完成；不额外清零百兆缓冲。 */
 return 1;
}
int rs_init_segments(RsCore *s,RsConfig c,const RsSegment *segments,unsigned count){
 size_t n;if(!s||!segments||!count||count>RS_MAX_SEGMENTS||!geometry(c,&n))return 0;
 uint32_t rows=0;size_t total=0;
 for(unsigned i=0;i<count;i++){
  RsConfig part=c;RsCore check;part.height=segments[i].rows;
  if(part.height>c.height-rows||!rs_init(&check,part,segments[i].workspace,segments[i].bytes))return 0;
  uintptr_t begin=(uintptr_t)segments[i].workspace;if(check.bytes>UINTPTR_MAX-begin)return 0;
  for(unsigned j=0;j<i;j++){
   RsConfig previous=c;size_t used=0;previous.height=segments[j].rows;
   if(!rs_workspace_bytes(previous,&used))return 0;
   uintptr_t prior=(uintptr_t)segments[j].workspace;
   if(begin<prior+used&&prior<begin+check.bytes)return 0;
  }
  rows+=part.height;total+=check.bytes;
 }
 if(rows!=c.height)return 0;
 *s=(RsCore){.config=c,.workspace=segments[0].workspace,.bytes=total,.samples=n,.segment_count=count};
 memcpy(s->segments,segments,count*sizeof *segments);return 1;
}
static RsCore segment_view(const RsCore *s,unsigned i){
 RsCore view=*s;view.segment_count=0;view.workspace=s->segments[i].workspace;
 view.config.height=s->segments[i].rows;view.samples=(size_t)view.config.height*view.config.stride_samples;
 view.bytes=s->segments[i].bytes;return view;
}
int rs_begin_frame(RsCore *s,size_t source_bytes){
 if(!s||s->failed||s->finished||s->frame_open||s->frames>=RS_MAX_FRAMES||
    source_bytes<s->samples*2)return 0;
 s->frame_open=1;return 1;
}
static int range(RsCore *s,uint32_t first,uint32_t rows,size_t *begin,size_t *end){
 if(!s||s->failed||!rows||first>=s->config.height||rows>s->config.height-first)return 0;
 *begin=(size_t)first*s->config.stride_samples;
 *end=*begin+(size_t)rows*s->config.stride_samples;return 1;
}
int rs_accumulate_rows(RsCore *s,const uint16_t *source,uint32_t first,uint32_t rows){
 if(!s||!source||first>=s->config.height)return 0;
 return rs_accumulate_block(s,source+(size_t)first*s->config.stride_samples,
  (size_t)rows*s->config.stride_samples*2,first,rows);
}
int rs_accumulate_block(RsCore *s,const uint16_t *source,size_t bytes,uint32_t first,uint32_t rows){
 size_t b,e;if(!source||((uintptr_t)source&1)||!s||s->frame_open!=1||!range(s,first,rows,&b,&e))return 0;
 if(bytes<(e-b)*2)return 0;
 if(s->segment_count){
  uint32_t start=0,end=first+rows;
  for(unsigned i=0;i<s->segment_count;i++){
   uint32_t next=start+s->segments[i].rows,lo=first>start?first:start,hi=end<next?end:next;
   if(lo<hi){RsCore view=segment_view(s,i);size_t offset=(size_t)(lo-first)*s->config.stride_samples;
    if(!rs_accumulate_block(&view,source+offset,(size_t)(hi-lo)*s->config.stride_samples*2,lo-start,hi-lo))return 0;
   }
   start=next;
  }
  return 1;
 }
 if(s->config.method==RS_AVERAGE){
  uint32_t *sum=s->workspace;
  if(!s->frames)for(size_t i=b;i<e;i++)sum[i]=source[i-b];
  else for(size_t i=b;i<e;i++)sum[i]+=source[i-b];
 }else if(s->config.noise_group){
  uint16_t *result=s->workspace;
  uint32_t *sum=(uint32_t *)((uint8_t *)s->workspace+((s->samples*2+7)&~(size_t)7));
  if(!s->pending_group)for(size_t i=b;i<e;i++)sum[i]=source[i-b];
  else for(size_t i=b;i<e;i++)sum[i]+=source[i-b];
  if(s->pending_group+1==s->config.noise_group){
   uint32_t count=s->config.noise_group;
   if(!s->extrema_ready)for(size_t i=b;i<e;i++)result[i]=(uint16_t)((sum[i]+count/2)/count);
   else if(s->config.method==RS_MAXIMUM)for(size_t i=b;i<e;i++){
    uint16_t v=(uint16_t)((sum[i]+count/2)/count);if(v>result[i])result[i]=v;
   }else for(size_t i=b;i<e;i++){
    uint16_t v=(uint16_t)((sum[i]+count/2)/count);if(v<result[i])result[i]=v;
   }
  }
 }else{
  uint16_t *result=s->workspace;
  if(!s->frames)memcpy(result+b,source,(e-b)*2);
  else if(s->config.method==RS_MAXIMUM)for(size_t i=b;i<e;i++){if(source[i-b]>result[i])result[i]=source[i-b];}
  else for(size_t i=b;i<e;i++){if(source[i-b]<result[i])result[i]=source[i-b];}
 }
 return 1;
}
int rs_commit_frame(RsCore *s){
 if(!s||s->failed||s->frame_open!=1||s->frames>=RS_MAX_FRAMES)return 0;
 s->frames++;s->frame_open=0;
 if(s->config.noise_group&&++s->pending_group==s->config.noise_group){s->pending_group=0;s->extrema_ready=1;}
 return 1;
}
int rs_begin_finish(RsCore *s){
 if(!s||s->failed||s->finished||s->frame_open||!s->frames||s->frames>RS_MAX_FRAMES)return 0;
 s->frame_open=2;return 1;
}
int rs_normalize_rows(RsCore *s,uint32_t first,uint32_t rows){
 size_t b,e;if(!s||s->frame_open!=2||!range(s,first,rows,&b,&e))return 0;
 if(s->segment_count){
  uint32_t start=0,end=first+rows;
  for(unsigned i=0;i<s->segment_count;i++){
   uint32_t next=start+s->segments[i].rows,lo=first>start?first:start,hi=end<next?end:next;
   if(lo<hi){RsCore view=segment_view(s,i);if(!rs_normalize_rows(&view,lo-start,hi-lo))return 0;}
   start=next;
  }
  return 1;
 }
 if(s->config.method==RS_AVERAGE){
  uint32_t *sum=s->workspace;
  for(size_t i=b;i<e;i++)sum[i]=(uint32_t)(((uint64_t)sum[i]+s->frames/2)/s->frames);
 }else if(s->pending_group){
  uint16_t *result=s->workspace;
  uint32_t *sum=(uint32_t *)((uint8_t *)s->workspace+((s->samples*2+7)&~(size_t)7)),n=s->pending_group;
  if(!s->extrema_ready)for(size_t i=b;i<e;i++)result[i]=(uint16_t)((sum[i]+n/2)/n);
  else if(s->config.method==RS_MAXIMUM)for(size_t i=b;i<e;i++){
   uint16_t v=(uint16_t)((sum[i]+n/2)/n);if(v>result[i])result[i]=v;
  }else for(size_t i=b;i<e;i++){
   uint16_t v=(uint16_t)((sum[i]+n/2)/n);if(v<result[i])result[i]=v;
  }
 }
 return 1;
}
const uint16_t *rs_compact(RsCore *s,size_t *bytes){
 if(!s||s->failed||s->finished||s->frame_open!=2||!bytes||s->segment_count)return NULL;
 if(s->config.method==RS_AVERAGE){
  /* 正向压紧，所有 normalize 工人已经 join；输出永远位于尚未读取数据之前。
   * memcpy 使用字节视图，避免 uint32/uint16 别名导致优化器破坏原地压紧。 */
  uint8_t *p=s->workspace;
  for(size_t i=0;i<s->samples;i++){uint32_t v;memcpy(&v,p+4*i,4);uint16_t out=(uint16_t)v;memcpy(p+2*i,&out,2);}
 }
 *bytes=s->samples*2;s->finished=1;s->frame_open=0;return s->workspace;
}
unsigned rs_compact_blocks(RsCore *s,RsPixelBlock *blocks,unsigned capacity,size_t *bytes){
 if(!s||!blocks||!bytes||s->failed||s->finished||s->frame_open!=2)return 0;
 unsigned count=s->segment_count?s->segment_count:1;if(capacity<count)return 0;
 if(!s->segment_count){const uint16_t *p=rs_compact(s,bytes);if(!p)return 0;blocks[0]=(RsPixelBlock){p,*bytes};return 1;}
 size_t total=0;
 for(unsigned i=0;i<count;i++){
  RsCore view=segment_view(s,i);size_t n=0;const uint16_t *p=rs_compact(&view,&n);
  if(!p){s->failed=1;return 0;}blocks[i]=(RsPixelBlock){p,n};total+=n;
 }
 *bytes=total;s->finished=1;s->frame_open=0;return count;
}
