/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 分块合成结果的双消费者交接核心。调用者持锁；不分配内存、不写文件、
 * 不控制相机。调用方缓冲引用由 release 回调释放，绝不因一条分支完成提前释放。
 * 这不是已接入相机的拍摄路径。 */
#ifndef PIXEL_CHUNK_FANOUT_H
#define PIXEL_CHUNK_FANOUT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define PC_SLOTS 4u
enum {PC_RAW=0, PC_RENDER=1, PC_CONSUMERS=2};
typedef struct {
 uint64_t task,ticket;
 uint32_t first_row,rows;
 const void *pixels;
 size_t bytes;
 void *owner;
 unsigned remaining,leased;
} PcChunk;
typedef struct {
 uint64_t task,next_ticket;
 uint32_t width,height,next_row,consumer_row[PC_CONSUMERS];
 unsigned failed,active[PC_CONSUMERS];
 PcChunk slots[PC_SLOTS];
 void (*release)(void *context,void *owner);
 void *context;
} PcFanout;
static inline int pc_init(PcFanout *p,uint64_t task,uint32_t width,uint32_t height,
 void (*release)(void *,void *),void *context){
 if(!p||!task||!width||!height||!release||width>UINT32_MAX/2)return 0;
 memset(p,0,sizeof *p);p->task=task;p->width=width;p->height=height;
 p->next_ticket=1;p->release=release;p->context=context;return 1;
}
/* 成功才转移 owner 引用。队列满返回 0，由生产者等待，不丢块、不无限分配。 */
static inline int pc_submit(PcFanout *p,uint32_t first_row,uint32_t rows,
 const void *pixels,size_t bytes,void *owner){
 if(!p||p->failed||!pixels||!owner||!rows||first_row!=p->next_row||
    first_row>p->height||rows>p->height-first_row||
    (uint64_t)rows*p->width*2!=bytes||p->next_ticket==UINT64_MAX)return 0;
 for(unsigned i=0;i<PC_SLOTS;i++)if(!p->slots[i].remaining){
  p->slots[i]=(PcChunk){p->task,p->next_ticket++,first_row,rows,pixels,bytes,owner,3,0};
  p->next_row+=rows;return 1;
 }
 return 0;
}
/* 每路同时只持有一块；各自严格按行顺序消费，但两路可以重叠运行。 */
static inline int pc_take(PcFanout *p,unsigned consumer,PcChunk *out){
 if(!p||!out||consumer>=PC_CONSUMERS||p->failed||p->active[consumer])return 0;
 unsigned bit=1u<<consumer;
 for(unsigned i=0;i<PC_SLOTS;i++){
  PcChunk *c=p->slots+i;
  if((c->remaining&bit)&&!(c->leased&bit)&&c->first_row==p->consumer_row[consumer]){
   c->leased|=bit;p->active[consumer]=1;*out=*c;return 1;
  }
 }
 return 0;
}
static inline void pc_release_finished(PcFanout *p){
 for(unsigned i=0;i<PC_SLOTS;i++){
  PcChunk *c=p->slots+i;
  if(c->owner&&!c->remaining){
   void *owner=c->owner;memset(c,0,sizeof *c);p->release(p->context,owner);
  }
 }
}
/* 取消只回收尚未交给消费者的引用；正在写入或显影的块等真实完成回调。 */
static inline void pc_abort(PcFanout *p){
 if(!p)return;p->failed=1;
 for(unsigned i=0;i<PC_SLOTS;i++)p->slots[i].remaining&=p->slots[i].leased;
 pc_release_finished(p);
}
static inline int pc_done(PcFanout *p,unsigned consumer,uint64_t task,uint64_t ticket,int ok){
 if(!p||consumer>=PC_CONSUMERS||task!=p->task)return 0;
 unsigned bit=1u<<consumer;
 for(unsigned i=0;i<PC_SLOTS;i++){
  PcChunk *c=p->slots+i;
  if(c->task==task&&c->ticket==ticket&&(c->remaining&bit)&&(c->leased&bit)){
   c->remaining&=~bit;c->leased&=~bit;p->active[consumer]=0;
   if(ok&&!p->failed)p->consumer_row[consumer]+=c->rows;
   if(!ok)pc_abort(p);else pc_release_finished(p);
   return 1;
  }
 }
 return 0;
}
static inline int pc_drained(const PcFanout *p){
 if(!p)return 0;
 for(unsigned i=0;i<PC_SLOTS;i++)if(p->slots[i].remaining||p->slots[i].owner)return 0;
 return !p->active[0]&&!p->active[1];
}
static inline int pc_complete(const PcFanout *p){
 return p&&!p->failed&&pc_drained(p)&&p->next_row==p->height&&
  p->consumer_row[0]==p->height&&p->consumer_row[1]==p->height;
}
#endif
