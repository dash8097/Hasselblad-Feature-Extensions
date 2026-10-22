/* 本次四亿 RAW 的顺序写入分支。调用者在独立写入线程串行调用；
 * 合成块写入完成后才向双路队列归还租约。没有原片或合成文件重读。
 * 头部由已有版本约束的 3FR 封装函数提供；真实元数据出口另行接入。 */
#ifndef CAPTURE_RAW_SINK_H
#define CAPTURE_RAW_SINK_H
#include "pixel_chunk_fanout.h"
#include "factory_native_3fr.h"
#include "photo_file_alignment.h"
typedef struct {
 int (*write)(void *,const void *,size_t);
 int (*finish)(void *,uint64_t);
 void (*abort)(void *);
 void *context;
} CrSinkIo;
typedef struct {
 CrSinkIo io;
 uint64_t task,last_ticket,bytes;
 uint32_t next_row;
 const uint8_t *preview;
 size_t preview_bytes;
 unsigned active,failed,complete;
} CrSink;
static inline void cr_sink_abort(CrSink *s){
 if(!s||!s->active)return;
 s->active=0;s->failed=1;s->io.abort(s->io.context);
 s->preview=NULL;s->preview_bytes=0;
}
/* preview 是独立复制后的原厂首帧预览，调用者保留到 finish/abort。
 * 不接受仍可能被原厂回收的无租约裸指针。 */
static inline int cr_sink_begin(CrSink *s,uint64_t task,CrSinkIo io,
 const uint8_t header[F3_HEADER],const uint8_t *preview,size_t preview_bytes){
 if(!s||s->active||s->complete||!task||!io.write||!io.finish||!io.abort||
    !header||!preview||!preview_bytes||preview_bytes>16u*1024u*1024u)return 0;
 uint8_t checked[F3_HEADER];memcpy(checked,header,sizeof checked);
 uint32_t first=f3_u32(checked+4),raw;
 if(memcmp(checked,"II*\0",4)||!f3_scalar(checked,sizeof checked,first,330,&raw)||
    !f3_is(checked,sizeof checked,raw,256,F3_WIDTH)||
    !f3_is(checked,sizeof checked,raw,257,F3_HEIGHT)||
    !f3_is(checked,sizeof checked,raw,258,16)||
    !f3_is(checked,sizeof checked,raw,259,1)||
    !f3_is(checked,sizeof checked,raw,273,F3_HEADER)||
    !f3_is(checked,sizeof checked,raw,279,F3_RAW_BYTES)||
    !f3_is(checked,sizeof checked,first,273,F3_HEADER+F3_RAW_BYTES)||
    !f3_is(checked,sizeof checked,first,279,(uint32_t)preview_bytes))return 0;
 memset(s,0,sizeof *s);s->io=io;s->task=task;s->preview=preview;
 s->preview_bytes=preview_bytes;s->active=1;
 if(!io.write(io.context,header,F3_HEADER)){cr_sink_abort(s);return 0;}
 s->bytes=F3_HEADER;return 1;
}
/* 同步写入只阻塞写入消费者；生产者与显影分支仍可运行。
 * 无效或重复块拒绝并中止本次半成品，不能静默跳行。 */
static inline int cr_sink_chunk(CrSink *s,const PcChunk *c){
 if(!s||!s->active||s->failed||!c)return 0;
 if(c->task!=s->task||!c->ticket||c->ticket<=s->last_ticket||
    c->first_row!=s->next_row||!c->rows||c->rows>4096||
    c->first_row>F3_HEIGHT||c->rows>F3_HEIGHT-c->first_row||!c->pixels||
    c->bytes!=(size_t)c->rows*F3_WIDTH*2){cr_sink_abort(s);return 0;}
 if(!s->io.write(s->io.context,c->pixels,c->bytes)){cr_sink_abort(s);return 0;}
 s->next_row+=c->rows;s->last_ticket=c->ticket;s->bytes+=c->bytes;return 1;
}
static inline int cr_sink_finish(CrSink *s){
 if(!s||!s->active||s->failed||s->next_row!=F3_HEIGHT||
    s->bytes!=F3_HEADER+(uint64_t)F3_RAW_BYTES){cr_sink_abort(s);return 0;}
 uint64_t total=s->bytes+s->preview_bytes;
 size_t padding=photo_eof_padding(total);const uint8_t zeros[4096]={0};
 if(!s->io.write(s->io.context,s->preview,s->preview_bytes)||
    (padding&&!s->io.write(s->io.context,zeros,padding))||
    !s->io.finish(s->io.context,total+padding)){cr_sink_abort(s);return 0;}
 s->bytes=total+padding;s->active=0;s->complete=1;s->preview=NULL;s->preview_bytes=0;
 return 1;
}
#endif
