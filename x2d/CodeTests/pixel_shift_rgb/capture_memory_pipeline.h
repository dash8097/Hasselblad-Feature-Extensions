/* 内存计算与双路队列的连接候选。所有入口由控制线程串行调用；
 * 计算分区在 dispatch 返回前 join。消费者完成须回到控制线程。
 * 输出可连接顺序文件消费者；原厂显影会话和真实拍摄仍须单独适配。 */
#ifndef CAPTURE_MEMORY_PIPELINE_H
#define CAPTURE_MEMORY_PIPELINE_H
#include "capture_memory_merge.h"
#include "capture_merge_workers.h"
#include "pixel_chunk_fanout.h"

typedef struct {
 uint16_t *pixels;
 size_t capacity;
 unsigned busy;
 unsigned first_row,rows; /* 已合成的完整周边区；pixels 是完整区起点。 */
} CmpBuffer;
typedef struct {
 CmView frames[6];
 PcFanout output;
 CmpBuffer buffers[PC_SLOTS];
 unsigned width,height,batch,next_row,source_finished;
 unsigned buffer_count,halo_before,halo_after;
 void *scratch;
 size_t scratch_bytes;
 CmMergeDispatch dispatch;
 void *dispatch_context;
 CmwWorkers *parallel;
 uint16_t *full_pixels;
 unsigned computed_row;
 void (*source_done)(void *,int);
 void *source_context;
} CmPipeline;
enum {CMP_FAILED=-1,CMP_WAIT=0,CMP_SUBMITTED=1,CMP_MERGED=2,CMP_CALCULATED=3};
static inline int cmp_overlaps(const void *a,size_t an,const void *b,size_t bn){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y||
        (x<y+bn&&y<x+an);
}

static inline void cmp_release_buffer(void *context,void *owner){
 CmPipeline *p=context;
 for(unsigned i=0;i<PC_SLOTS;i++)if(owner==p->buffers+i){
  p->buffers[i].busy=0;return;
 }
}
static inline void cmp_finish_sources(CmPipeline *p,int ok){
 if(!p->source_finished){
  p->source_finished=1;
  /* 调用点属于生产者线程，原厂引用释放仍由其 QObject 线程适配完成。 */
  p->source_done(p->source_context,ok);
  memset(p->frames,0,sizeof p->frames);
 }
}
static inline int cmp_init_window(CmPipeline *p,uint64_t task,const CmView frames[6],
 unsigned width,unsigned height,unsigned batch,CmpBuffer buffers[PC_SLOTS],
 unsigned buffer_count,unsigned halo_before,unsigned halo_after,
 void *scratch,size_t scratch_bytes,CmMergeDispatch dispatch,void *dispatch_context,
 void (*source_done)(void *,int),void *source_context){
 if(!p||!task||!frames||!buffers||!width||width>11663||!height||height>8749||
    !batch||batch>2048||!buffer_count||buffer_count>PC_SLOTS||
    halo_before>64||halo_after>64||!scratch||scratch_bytes<cm_merge_scratch_bytes(width)||
    !source_done)return 0;
 unsigned capacity_rows=batch+halo_before+halo_after;
 if(capacity_rows>height)capacity_rows=height;
 size_t bytes=(size_t)width*capacity_rows*8;
 for(unsigned i=0;i<buffer_count;i++){
  if(!buffers[i].pixels||buffers[i].capacity<bytes||buffers[i].busy||
     ((uintptr_t)buffers[i].pixels&1))return 0;
  if(cmp_overlaps(buffers[i].pixels,buffers[i].capacity,scratch,scratch_bytes))return 0;
  for(unsigned j=0;j<i;j++)if(cmp_overlaps(buffers[i].pixels,buffers[i].capacity,
     buffers[j].pixels,buffers[j].capacity))return 0;
  for(unsigned j=0;j<6;j++)if(!frames[j].data||
     cmp_overlaps(buffers[i].pixels,buffers[i].capacity,frames[j].data,frames[j].bytes)||
     cmp_overlaps(scratch,scratch_bytes,frames[j].data,frames[j].bytes))return 0;
 }
 memset(p,0,sizeof *p);
 if(!pc_init(&p->output,task,width*2,height*2,cmp_release_buffer,p))return 0;
 memcpy(p->frames,frames,sizeof p->frames);
 memcpy(p->buffers,buffers,buffer_count*sizeof *buffers);
 p->width=width;p->height=height;p->batch=batch;p->scratch=scratch;
 p->buffer_count=buffer_count;p->halo_before=halo_before;p->halo_after=halo_after;
 p->scratch_bytes=scratch_bytes;p->dispatch=dispatch;p->dispatch_context=dispatch_context;
 p->source_done=source_done;p->source_context=source_context;return 1;
}
static inline int cmp_init(CmPipeline *p,uint64_t task,const CmView frames[6],
 unsigned width,unsigned height,unsigned batch,CmpBuffer buffers[PC_SLOTS],
 void *scratch,size_t scratch_bytes,CmMergeDispatch dispatch,void *dispatch_context,
 void (*source_done)(void *,int),void *source_context){
 return cmp_init_window(p,task,frames,width,height,batch,buffers,PC_SLOTS,0,0,
  scratch,scratch_bytes,dispatch,dispatch_context,source_done,source_context);
}
/* 整幅结果由外部共享图像池持有；分块仅用于计算调度和后台顺序写入。
 * 全部计算结束并归还六帧后才交给消费者，输出无需为复用缓冲等待写盘。 */
static inline int cmp_init_full(CmPipeline *p,uint64_t task,const CmView frames[6],
 unsigned width,unsigned height,unsigned batch,uint16_t *pixels,size_t bytes,
 void *scratch,size_t scratch_bytes,void (*source_done)(void *,int),void *context){
 if(!p||!task||!frames||!width||width>11663||!height||height>8749||
    !batch||batch>1024||!pixels||((uintptr_t)pixels&1)||bytes!=(size_t)width*height*8||
    !scratch||scratch_bytes<cm_merge_scratch_bytes(width)||!source_done)return 0;
 if(cmp_overlaps(pixels,bytes,scratch,scratch_bytes))return 0;
 for(unsigned i=0;i<6;i++)if(!frames[i].data||cmp_overlaps(pixels,bytes,frames[i].data,frames[i].bytes))return 0;
 memset(p,0,sizeof *p);
 if(!pc_init(&p->output,task,width*2,height*2,cmp_release_buffer,p))return 0;
 memcpy(p->frames,frames,sizeof p->frames);p->full_pixels=pixels;
 p->width=width;p->height=height;p->batch=batch;p->buffer_count=PC_SLOTS;
 p->scratch=scratch;p->scratch_bytes=scratch_bytes;p->source_done=source_done;p->source_context=context;
 return 1;
}
static inline void cmp_abort(CmPipeline *p){
 if(!p)return;
 pc_abort(&p->output);
 cmp_finish_sources(p,0);
}
/* 只计算一个块；队列满则等待。没有等待整张完成或等待写入分支的关卡。 */
static inline int cmp_step(CmPipeline *p){
 if(!p||p->output.failed)return CMP_FAILED;
 if(p->full_pixels&&p->computed_row<p->height){
  unsigned rows=p->height-p->computed_row;if(rows>p->batch)rows=p->batch;
  unsigned end=p->computed_row+rows;
  for(unsigned at=p->computed_row;at<end;){
   unsigned limit=p->parallel?cmw_capacity(p->parallel):1024;
   unsigned count=end-at;if(!limit){cmp_abort(p);return CMP_FAILED;}if(count>limit)count=limit;
   uint16_t *to=p->full_pixels+(size_t)at*p->width*4;size_t bytes=(size_t)count*p->width*8;
   int ok=p->parallel?cmw_run(p->parallel,p->frames,p->height,at,count,to,bytes):
    cm_merge_rows(p->frames,p->width,p->height,at,count,to,bytes,p->scratch,p->scratch_bytes,NULL,NULL);
   if(!ok){cmp_abort(p);return CMP_FAILED;}at+=count;
  }
  p->computed_row=end;if(end==p->height)cmp_finish_sources(p,1);
  return CMP_CALCULATED;
 }
 if(p->next_row==p->height)return CMP_MERGED;
 CmpBuffer *buffer=NULL;
 for(unsigned i=0;i<p->buffer_count;i++)if(!p->buffers[i].busy){buffer=p->buffers+i;break;}
 if(!buffer)return CMP_WAIT;
 unsigned rows=p->height-p->next_row;if(rows>p->batch)rows=p->batch;
 size_t bytes=(size_t)p->width*rows*8;
 if(p->full_pixels){
  buffer->pixels=p->full_pixels+(size_t)p->next_row*p->width*4;buffer->capacity=bytes;
  buffer->first_row=p->next_row*2;buffer->rows=rows*2;buffer->busy=1;
  if(!pc_submit(&p->output,p->next_row*2,rows*2,buffer->pixels,bytes,buffer)){
   buffer->busy=0;cmp_abort(p);return CMP_FAILED;
  }
  p->next_row+=rows;return CMP_SUBMITTED;
 }
 unsigned first=p->next_row>p->halo_before?p->next_row-p->halo_before:0;
 unsigned end=p->next_row+rows;
 end+=p->height-end<p->halo_after?p->height-end:p->halo_after;
 for(unsigned at=first;at<end;){
  unsigned limit=p->parallel?cmw_capacity(p->parallel):1024;
  if(!limit){cmp_abort(p);return CMP_FAILED;}
  unsigned count=end-at;if(count>limit)count=limit;
  int ok=p->parallel?cmw_run(p->parallel,p->frames,p->height,at,count,
    buffer->pixels+(size_t)(at-first)*p->width*4,(size_t)p->width*count*8):
   cm_merge_rows(p->frames,p->width,p->height,at,count,
    buffer->pixels+(size_t)(at-first)*p->width*4,(size_t)p->width*count*8,
    p->scratch,p->scratch_bytes,p->dispatch,p->dispatch_context);
  if(!ok){
   cmp_abort(p);return CMP_FAILED;
  }
  at+=count;
 }
 buffer->first_row=first*2;buffer->rows=(end-first)*2;
 buffer->busy=1;
 const uint16_t *valid=buffer->pixels+(size_t)(p->next_row-first)*p->width*4;
 if(!pc_submit(&p->output,p->next_row*2,rows*2,valid,bytes,buffer)){
  buffer->busy=0;cmp_abort(p);return CMP_FAILED;
 }
 p->next_row+=rows;
 if(p->next_row==p->height)cmp_finish_sources(p,1);
 return CMP_SUBMITTED;
}
static inline int cmp_take(CmPipeline *p,unsigned consumer,PcChunk *out){
 return p&&pc_take(&p->output,consumer,out);
}
static inline int cmp_done(CmPipeline *p,unsigned consumer,uint64_t task,
 uint64_t ticket,int ok){
 if(!p)return 0;
 int accepted=pc_done(&p->output,consumer,task,ticket,ok);
 if(accepted&&!ok)cmp_finish_sources(p,0);
 return accepted;
}
static inline int cmp_complete(const CmPipeline *p){
 return p&&p->source_finished&&pc_complete(&p->output);
}
#endif
