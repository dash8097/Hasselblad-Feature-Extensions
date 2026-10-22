/* 六张拍摄共享图像的内存消费候选。调用者串行化通知与租约操作。
 * 不安装 hook、不修改原厂写入/通知完成状态、不拍摄或分配图像池。
 * retain/release 适配必须保持 ImageMemory 的真实强引用。 */
#ifndef CAPTURE_MEMORY_INPUT_H
#define CAPTURE_MEMORY_INPUT_H
#include "private_capture_group.h"
#include "private_capture_raw_profile.h"
#include "capture_preview_stream.h"
#include <string.h>
typedef struct {
 uint32_t width,height,stride;
 uint8_t format;
 const uint8_t *data;
 size_t bytes;
 int owns_allocation;
} CmView;
typedef struct {
 int (*retain)(void *,const X2dQtShared *,X2dQtShared *);
 void (*release)(void *,X2dQtShared *);
 int (*view)(void *,const X2dQtShared *,CmView *);
 void *context;
 int (*allowed)(void *); /* 原厂适配要求存储 QObject 线程；纯主机替身可为空。 */
} CmApi;
typedef struct {
 X2dQtShared image;
 CmView view;
 uint32_t next_row,leased_rows;
 uint64_t ticket;
 unsigned received,active;
} CmFrame;
typedef struct {
 X2dCaptureGroup group;
 CmApi api;
 CmFrame frames[6],preview;
 uint64_t next_ticket;
 unsigned failed,preview_copied;
 unsigned merge_active;
 uint64_t merge_ticket;
} CmInput;
typedef struct {
 uint64_t task,ticket;
 unsigned index;
 uint32_t first_row,rows,stride;
 const uint8_t *data;
 size_t bytes;
} CmSlice;
static inline int cm_allowed(const CmInput *p){
 return p&&(!p->api.allowed||p->api.allowed(p->api.context));
}
static inline void cm_drop(CmInput *p,CmFrame *f){
 if(!cm_allowed(p))return;
 if(f->image.control)p->api.release(p->api.context,&f->image);
 memset(&f->image,0,sizeof f->image);memset(&f->view,0,sizeof f->view);
}
static inline int cm_init(CmInput *p,uint64_t task,CmApi api){
 if(!p||!api.retain||!api.release||!api.view||!task||(api.allowed&&!api.allowed(api.context)))return 0;
 memset(p,0,sizeof *p);p->api=api;p->next_ticket=1;
 return x2d_capture_group_arm(&p->group,task,6);
}
static inline void cm_abort(CmInput *p){
 if(!cm_allowed(p))return;p->failed=p->group.failed=1;
 for(unsigned i=0;i<6;i++)if(!p->frames[i].active)cm_drop(p,p->frames+i);
 if(!p->preview.active)cm_drop(p,&p->preview);
}
/* 只有后端已发出的本任务请求可认领。普通图像返回 NORMAL 给原函数处理。
 * 只观察/保留真实 RAW 与第一张预览；不伪造原厂存储完成通知。 */
static inline int cm_receive(CmInput *p,const X2dExposureInfo *info,
 const X2dCaptureIdentity *identity,int encoded_preview_complete){
 if(!cm_allowed(p)||!info)return X2D_CAPTURE_CONFLICT;
 unsigned index;
 int c=x2d_capture_group_classify(&p->group,info->exposure_id,identity,&index);
 if(c==X2D_CAPTURE_NORMAL)return c;
 if(c!=X2D_CAPTURE_OWN||p->failed){cm_abort(p);return X2D_CAPTURE_CONFLICT;}
 if(info->kind!=1&&info->kind!=0)return c;
 if(info->kind==0&&index!=0)return c;
 CmFrame *f=info->kind==1?p->frames+index:&p->preview;
 if(f->received||!info->image.value||!info->image.control||
    (info->kind==0&&!encoded_preview_complete)){
  cm_abort(p);return X2D_CAPTURE_CONFLICT;
 }
 X2dQtShared retained={0};CmView v={0};
 int ok=p->api.retain(p->api.context,&info->image,&retained);
 ok=ok&&retained.value&&retained.control&&p->api.view(p->api.context,&retained,&v);
 if(ok&&info->kind==1){
  ok=x2d_capture_raw_geometry(v.width,v.height,v.format,v.stride)==X2D_CAPTURE_RAW_VALID&&
     v.bytes<=INTPTR_MAX&&x2d_capture_raw_view(v.owns_allocation,v.data!=NULL,(intptr_t)v.bytes)==X2D_CAPTURE_RAW_VALID;
 }else if(ok){
  ok=v.format==8&&v.width==3888&&v.height==2918&&!v.owns_allocation;
  size_t bytes=ok?capture_preview_stream_size(v.data,v.bytes):0;
  ok=bytes!=0;v.bytes=bytes;
 }
 if(!ok){
  if(retained.control)p->api.release(p->api.context,&retained);
  cm_abort(p);return X2D_CAPTURE_CONFLICT;
 }
 f->image=retained;f->view=v;f->received=1;return c;
}
/* 第一张预览可以在其余五张 RAW 还未收到时复制。完成后由消费者持有副本，
 * 同一副本供最终 3FR/JPEG 封装，原厂预览引用立即释放。 */
static inline int cm_preview_take(CmInput *p,CmSlice *out){
 if(!cm_allowed(p)||!out||p->failed||p->preview_copied||!p->preview.image.control||
    p->preview.active||p->next_ticket==UINT64_MAX)return 0;
 CmFrame *f=&p->preview;f->ticket=p->next_ticket++;f->active=1;
 *out=(CmSlice){p->group.job,f->ticket,0,0,0,0,f->view.data,f->view.bytes};return 1;
}
static inline int cm_preview_done(CmInput *p,uint64_t task,uint64_t ticket,int copied){
 if(!cm_allowed(p)||task!=p->group.job||!p->preview.active||ticket!=p->preview.ticket)return 0;
 p->preview.active=0;
 if(copied&&!p->failed)p->preview_copied=1;
 cm_drop(p,&p->preview);if(!copied)cm_abort(p);return 1;
}
/* 各帧按行独立消费，最多 1024 行。借用原厂内存，不从 SSD 重读。
 * 最后一个租约完成后才释放整帧；不能把部分行当作可单独 free 的原厂块。 */
static inline int cm_rows_take(CmInput *p,unsigned index,unsigned rows,CmSlice *out){
 if(!cm_allowed(p)||!out||p->failed||p->merge_active||index>=6||!rows||rows>1024||p->next_ticket==UINT64_MAX)return 0;
 CmFrame *f=p->frames+index;
 if(!f->image.control||f->active||f->next_row>=f->view.height)return 0;
 if(rows>f->view.height-f->next_row)rows=f->view.height-f->next_row;
 f->ticket=p->next_ticket++;f->active=1;f->leased_rows=rows;
 size_t offset=(size_t)f->next_row*f->view.stride,bytes=(size_t)rows*f->view.stride;
 *out=(CmSlice){p->group.job,f->ticket,index,f->next_row,rows,f->view.stride,f->view.data+offset,bytes};
 return 1;
}
static inline int cm_rows_done(CmInput *p,unsigned index,uint64_t task,uint64_t ticket,int ok){
 if(!cm_allowed(p)||p->merge_active||index>=6||task!=p->group.job)return 0;
 CmFrame *f=p->frames+index;
 if(!f->active||f->ticket!=ticket)return 0;
 f->active=0;
 if(ok&&!p->failed)f->next_row+=f->leased_rows;
 f->leased_rows=0;
 if(!ok)cm_abort(p);
 if(p->failed||f->next_row==X2D_CAPTURE_RAW_HEIGHT)cm_drop(p,f);
 return 1;
}
/* 合成需要跨源行的周边采样，所以六张整帧使用同一租约。
 * 内存池分配不能按已读行释放；仅在全部计算线程 join 后交回 owner 线程。
 * 不改 group.written，不冒充原厂 RAW 已写入。 */
static inline int cm_merge_take(CmInput *p,CmView views[6],uint64_t *ticket){
 if(!cm_allowed(p)||!views||!ticket||p->failed||p->merge_active||
    p->group.seen!=63||p->next_ticket==UINT64_MAX)return 0;
 for(unsigned i=0;i<6;i++){
  CmFrame *f=p->frames+i;
  if(!f->received||!f->image.control||f->active||f->next_row)return 0;
 }
 p->merge_ticket=p->next_ticket++;p->merge_active=1;*ticket=p->merge_ticket;
 for(unsigned i=0;i<6;i++){p->frames[i].active=1;views[i]=p->frames[i].view;}
 return 1;
}
static inline int cm_merge_done(CmInput *p,uint64_t task,uint64_t ticket,int ok){
 if(!cm_allowed(p)||!p->merge_active||task!=p->group.job||ticket!=p->merge_ticket)return 0;
 p->merge_active=0;
 for(unsigned i=0;i<6;i++){
  CmFrame *f=p->frames+i;f->active=0;
  if(ok&&!p->failed)f->next_row=f->view.height;
  cm_drop(p,f);
 }
 if(!ok)cm_abort(p);return 1;
}
static inline int cm_drained(const CmInput *p){
 if(!p)return 0;
 for(unsigned i=0;i<6;i++)if(p->frames[i].active||p->frames[i].image.control)return 0;
 return !p->preview.active&&!p->preview.image.control;
}
static inline int cm_consumed(const CmInput *p){
 if(!p||p->failed||!p->preview_copied||!cm_drained(p)||p->group.seen!=63)return 0;
 for(unsigned i=0;i<6;i++)if(p->frames[i].next_row!=X2D_CAPTURE_RAW_HEIGHT)return 0;
 return 1;
}
#endif
