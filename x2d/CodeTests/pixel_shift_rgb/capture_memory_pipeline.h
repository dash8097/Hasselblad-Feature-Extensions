/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 内存计算与双路队列的连接候选。所有入口由控制线程串行调用；
 * 计算分区在 dispatch 返回前 join。消费者完成须回到控制线程。
 * 不连接真实拍摄、3FR 文件封装或原厂显影会话。 */
#ifndef CAPTURE_MEMORY_PIPELINE_H
#define CAPTURE_MEMORY_PIPELINE_H
#include "capture_memory_merge.h"
#include "pixel_chunk_fanout.h"

typedef struct {
 uint16_t *pixels;
 size_t capacity;
 unsigned busy;
} CmpBuffer;
typedef struct {
 CmView frames[6];
 PcFanout output;
 CmpBuffer buffers[PC_SLOTS];
 unsigned width,height,batch,next_row,source_finished;
 void *scratch;
 size_t scratch_bytes;
 CmMergeDispatch dispatch;
 void *dispatch_context;
 void (*source_done)(void *,int);
 void *source_context;
} CmPipeline;
enum {CMP_FAILED=-1,CMP_WAIT=0,CMP_SUBMITTED=1,CMP_MERGED=2};
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
  /* 调用点属于生产者线程；调用者在此结束借用输入的生命周期。 */
  p->source_done(p->source_context,ok);
  memset(p->frames,0,sizeof p->frames);
 }
}
static inline int cmp_init(CmPipeline *p,uint64_t task,const CmView frames[6],
 unsigned width,unsigned height,unsigned batch,CmpBuffer buffers[PC_SLOTS],
 void *scratch,size_t scratch_bytes,CmMergeDispatch dispatch,void *dispatch_context,
 void (*source_done)(void *,int),void *source_context){
 if(!p||!task||!frames||!buffers||!width||width>11663||!height||height>8749||
    !batch||batch>1024||!scratch||scratch_bytes<cm_merge_scratch_bytes(width)||
    !source_done)return 0;
 size_t bytes=(size_t)width*(batch<height?batch:height)*8;
 for(unsigned i=0;i<PC_SLOTS;i++){
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
 memcpy(p->frames,frames,sizeof p->frames);memcpy(p->buffers,buffers,sizeof p->buffers);
 p->width=width;p->height=height;p->batch=batch;p->scratch=scratch;
 p->scratch_bytes=scratch_bytes;p->dispatch=dispatch;p->dispatch_context=dispatch_context;
 p->source_done=source_done;p->source_context=source_context;return 1;
}
static inline void cmp_abort(CmPipeline *p){
 if(!p)return;
 pc_abort(&p->output);
 cmp_finish_sources(p,0);
}
/* 只计算一个块；队列满则等待。没有等待整张完成或等待写入分支的关卡。 */
static inline int cmp_step(CmPipeline *p){
 if(!p||p->output.failed)return CMP_FAILED;
 if(p->next_row==p->height)return CMP_MERGED;
 CmpBuffer *buffer=NULL;
 for(unsigned i=0;i<PC_SLOTS;i++)if(!p->buffers[i].busy){buffer=p->buffers+i;break;}
 if(!buffer)return CMP_WAIT;
 unsigned rows=p->height-p->next_row;if(rows>p->batch)rows=p->batch;
 size_t bytes=(size_t)p->width*rows*8;
 if(!cm_merge_rows(p->frames,p->width,p->height,p->next_row,rows,buffer->pixels,
                   bytes,p->scratch,p->scratch_bytes,p->dispatch,p->dispatch_context)){
  cmp_abort(p);return CMP_FAILED;
 }
 buffer->busy=1;
 if(!pc_submit(&p->output,p->next_row*2,rows*2,buffer->pixels,bytes,buffer)){
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
