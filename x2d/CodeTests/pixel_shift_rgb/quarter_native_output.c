#define _POSIX_C_SOURCE 200809L
#include "quarter_native_output.h"
#ifndef QN_THREAD_EXTERNAL
#include <pthread.h>
#include <time.h>
#endif
#include <stdint.h>
#include <string.h>
#include "photo_file_alignment.h"
#include "factory_native_3fr.h"
typedef struct {
 const QnRawInput *in;QnRawOutput *out;
 uint64_t ends[RS_MAX_SEGMENTS];
 unsigned first,rows,finished;
} QnRows;
/* 原厂内存段可在一行中间结束。只对跨段的这一行使用工人私有小缓存；
 * 完整行直接借用原缓冲，不新增整张源图复制，不在像素循环中查段边界。 */
static const uint16_t *input_row(QnRows *w,unsigned row,unsigned *segment,uint16_t *scratch){
 uint64_t offset=(uint64_t)row*w->in->stride*2;
 size_t bytes=(size_t)w->in->width*2;
 while(offset>=w->ends[*segment])(*segment)++;
 uint64_t start=*segment?w->ends[*segment-1]:0;
 const uint8_t *data=(const uint8_t*)w->in->blocks[*segment].pixels+(size_t)(offset-start);
 if(w->ends[*segment]-offset>=bytes)return (const uint16_t*)data;
 unsigned at=*segment;size_t copied=0;
 while(copied<bytes){
  size_t available=(size_t)(w->ends[at]-offset);
  size_t n=bytes-copied<available?bytes-copied:available;
  memcpy((uint8_t*)scratch+copied,data,n);copied+=n;offset+=n;
  if(copied<bytes){at++;data=(const uint8_t*)w->in->blocks[at].pixels;}
 }
 return scratch;
}
static void *convert_rows(void *context){
 QnRows *w=context;
 unsigned segment_a=0,segment_b=0;
 uint16_t scratch_a[11904],scratch_b[11904];
 for(unsigned y=w->first;y<w->first+w->rows;y++){
  unsigned sy=4*(y/2)+(y&1),by=sy+2<w->in->height?sy+2:sy;
  const uint16_t *a=input_row(w,sy,&segment_a,scratch_a);
  const uint16_t *b=input_row(w,by,&segment_b,scratch_b);
  uint16_t *out=w->out->pixels+(size_t)y*w->out->stride;
  for(unsigned x=0;x<w->out->width;x++){
   unsigned sx=4*(x/2)+(x&1);
   out[x]=(uint16_t)(((uint32_t)a[sx]+a[sx+2]+b[sx]+b[sx+2]+2)/4);
  }
  memset(out+w->out->width,0,(w->out->stride-w->out->width)*sizeof *out);
 }
 __atomic_store_n(&w->finished,1,__ATOMIC_RELEASE);return NULL;
}
int qn_raw_convert(const QnRawInput *in,QnRawOutput *out,unsigned workers){
 if(!in||!out||!in->blocks||!in->count||in->count>RS_MAX_SEGMENTS||
    !in->width||in->width>11904||!in->height||in->stride<in->width||!workers||workers>4||
    (in->width%4)||(in->height%2)||out->width!=in->width/2||out->height!=in->height/2||
    out->stride<out->width||!out->pixels||((uintptr_t)out->pixels&1))return 0;
 uint64_t capacity=(uint64_t)out->stride*out->height*2;
 uintptr_t destination=(uintptr_t)out->pixels;
 if(capacity>SIZE_MAX||capacity>UINTPTR_MAX-destination||out->bytes<capacity)return 0;
 QnRows lanes[4];uint64_t ends[RS_MAX_SEGMENTS],total=0;
 uint64_t line=(uint64_t)in->stride*2;
 if(in->height>UINT64_MAX/line)return 0;
 uint64_t required=line*in->height;
 for(unsigned i=0;i<in->count;i++){
  const RsPixelBlock *p=in->blocks+i;uintptr_t source=(uintptr_t)p->pixels;
  if(!p->pixels||(source&1)||!p->bytes||(p->bytes&1)||p->bytes>UINTPTR_MAX-source||
     !(source+p->bytes<=destination||destination+capacity<=source))return 0;
  if(p->bytes>required-total)return 0;
  total+=p->bytes;ends[i]=total;
 }
 if(total!=required)return 0;
 for(unsigned i=0;i<workers;i++){
  unsigned first=(uint64_t)out->height*i/workers,end=(uint64_t)out->height*(i+1)/workers;
  lanes[i]=(QnRows){.in=in,.out=out,.first=first,.rows=end-first};
  memcpy(lanes[i].ends,ends,in->count*sizeof *ends);
 }
 pthread_t threads[3];unsigned started=0;int ok=1;
 for(unsigned i=0;i+1<workers;i++){
  if(pthread_create(threads+i,NULL,convert_rows,lanes+i))break;
  started++;
 }
 for(unsigned i=started;i<workers;i++)convert_rows(lanes+i);
 for(unsigned i=0;i<started;i++)if(pthread_join(threads[i],NULL)){
  /* 不允许 join 异常使输入借用提前结束；仍等到该工人停止访问。 */
  struct timespec pause={0,1000000};
  while(!__atomic_load_n(&lanes[i].finished,__ATOMIC_ACQUIRE))nanosleep(&pause,NULL);
  ok=0;
 }
 return ok;
}
int qn_exposure_output(QnExposureOutput *out,QnResolution resolution,unsigned width,unsigned height){
 if(!out||!width||!height||(resolution!=QN_NATIVE_SIZE&&resolution!=QN_QUARTER_SIZE)||
    (resolution==QN_QUARTER_SIZE&&((width&1)||(height&1))))return 0;
 *out=(QnExposureOutput){resolution,width/(resolution==QN_QUARTER_SIZE?2:1),height/(resolution==QN_QUARTER_SIZE?2:1)};
 return 1;
}
int qn_raw_plan_storage(const uint8_t *source,size_t raw_storage,size_t preview,size_t preview_storage,QnRawPlan *plan){
 if(!plan||raw_storage<52627584||raw_storage>52627584+4095||
    preview_storage<preview||preview_storage>16u*1024*1024+4095)return 0;
 QnRawPlan candidate={.raw_offset=16384,.raw_bytes=52627584,.preview_offset=52643968,
  .raw_storage_bytes=raw_storage,.preview_bytes=preview,.preview_storage_bytes=preview_storage,
  .stored_width=5952,.stored_height=4421,.effective_width=5828,.effective_height=4371};
 if(!qm_raw_header(source,preview,candidate.header))return 0;
 candidate.preview_offset=candidate.raw_offset+raw_storage;
 if(!f3_put32(candidate.header,f3_u32(candidate.header+4),273,(uint32_t)candidate.preview_offset))return 0;
 candidate.padding=photo_eof_padding(candidate.preview_offset+preview_storage);
 candidate.total_bytes=candidate.preview_offset+preview_storage+candidate.padding;
 *plan=candidate;return 1;
}
int qn_raw_plan(const uint8_t *source,size_t preview,QnRawPlan *plan){
 return qn_raw_plan_storage(source,52627584,preview,preview,plan);
}
int qn_raw_work_prepare(const QnRawInput *input,const uint8_t *header,size_t preview,QnWriteAdapter api){
 QnRawPlan plan;QnOutputLease lease={0};int ok=0;
 if(!input||input->width!=11904||input->height!=8842||input->stride!=11904||
    !api.allocate||!api.release||!api.commit||
    !qn_raw_plan_storage(header,52627584,preview,
     api.preview_storage_bytes?api.preview_storage_bytes:preview,&plan))return 0;
 if(!api.allocate(api.context,(size_t)plan.raw_bytes,&lease)){
  if(lease.native_owner||lease.output.pixels)api.release(api.context,&lease);
  return 0;
 }
 if(!lease.native_owner||lease.output.width!=5952||lease.output.height!=4421||lease.output.stride!=5952||
    !qn_raw_plan_storage(header,lease.output.bytes,preview,
     api.preview_storage_bytes?api.preview_storage_bytes:preview,&plan)||
    !qn_raw_convert(input,&lease.output,4))goto finish;
 memset((uint8_t*)lease.output.pixels+plan.raw_bytes,0,(size_t)(plan.raw_storage_bytes-plan.raw_bytes));
 ok=api.commit(api.context,&plan,&lease);
 /* 原厂绑定必须明确移动输出引用，不允许“返回成功但没持有数据”。 */
 if(ok&&(lease.native_owner||lease.output.pixels))ok=0;
finish:
 if(lease.native_owner||lease.output.pixels)api.release(api.context,&lease);
 return ok;
}
