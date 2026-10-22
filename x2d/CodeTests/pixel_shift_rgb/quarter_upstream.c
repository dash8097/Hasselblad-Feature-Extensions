#include "quarter_upstream.h"
#include <string.h>
static uint32_t u32(const void *p,unsigned o){uint32_t v;memcpy(&v,(const uint8_t*)p+o,4);return v;}
static void *ptr(const void *p,unsigned o){void *v;memcpy(&v,(const uint8_t*)p+o,sizeof v);return v;}
static void put32(void *p,unsigned o,uint32_t v){memcpy((uint8_t*)p+o,&v,4);}
int qdu_exposure(const void *engine,uint32_t frame,uintptr_t expected,uint32_t *id){
 if(!engine||!id||!expected)return 0;
 const void *map=ptr(engine,0x220);if(!map)return 0;
 const void *node=ptr(map,0x10);
 for(unsigned depth=0;node&&depth<64;depth++){
  uint32_t key=u32(node,0x20);
  if(key==frame){
   const void *functor=ptr(node,0x30+0x40);
   if(!functor||(uintptr_t)ptr(functor,0)!=expected)return 0;
   *id=u32(functor,0x24);return 1;
  }
  node=ptr(node,key<frame?8:0);
 }
 return 0;
}
static int geometry(const void *frame,uint32_t bytes){
 uint32_t stride=u32(frame,0x40);uint64_t plane=(uint64_t)stride*8742;
 return u32(frame,0x28)==103&&u32(frame,0x38)==11656&&u32(frame,0x3c)==8742&&
  stride>=11656&&!(stride&255)&&plane*2<=bytes&&u32(frame,0x80)==2&&
  u32(frame,0x44)==0&&u32(frame,0x48)==8742&&u32(frame,0x4c)==plane&&
  u32(frame,0x50)==stride&&u32(frame,0x54)==plane&&u32(frame,0x58)==8742&&u32(frame,0x5c)==plane;
}
int qdu_encode(QduState *s,const QduOps *o,void *encoder,QduRequest *r){
 if(!s||!o||!o->encode)return -1001;
 void *owned=0,*srcmap=0,*dstmap=0,*input=0;uint32_t input_bytes=0;
 int src_mapped=0,dst_mapped=0;
 if(!r||!r->frame||!r->stream)return -1001;
 if(!o->eligible||!o->size||!o->map||!o->unmap||!o->sync||!o->allocate||!o->release||
    s->blocked||!o->eligible(o->context,r->frame))goto fallback;
 input=ptr(r->frame,0x20);
 if(!input||o->size(input,&input_bytes)||!geometry(r->frame,input_bytes)||
    u32(r->stream,0x28)!=8||!ptr(r->stream,0x20)||!u32(r->stream,0x38))goto fallback;
 if(o->allocate(o->context,&owned,QD_BYTES)||!owned)goto cleanup;
 {uint32_t bytes=0;if(o->size(owned,&bytes)||bytes<QD_BYTES)goto cleanup;}
 if(o->map(input,&srcmap))goto cleanup;src_mapped=1;
 if(!srcmap||o->sync(input,1))goto cleanup;
 if(o->map(owned,&dstmap))goto cleanup;dst_mapped=1;
 if(!dstmap)goto cleanup;
 {
  QdNv16 source={srcmap,input_bytes,11656,8742,u32(r->frame,0x40)};
  if(!qd_resize(&source,dstmap,QD_BYTES)||o->sync(owned,2))goto cleanup;
 }
 /* CPU访问结束，HAL同步调用期间只保留设备内存引用。 */
 src_mapped=0;if(o->unmap(input)){s->blocked=1;goto cleanup;}
 dst_mapped=0;if(o->unmap(owned)){s->blocked=1;s->quarantined=owned;owned=0;goto cleanup;}
 {
  _Alignas(16) uint8_t frame[0x218],stream[0x1c8],unused[0x1c8];
  if(!qd_descriptors(frame,unused,owned,ptr(r->stream,0x20),u32(r->stream,0x38)))goto cleanup;
  /* 保留原厂输出池、偏移、标志；仅替换私有描述符中的尺寸。 */
  memcpy(stream,r->stream,sizeof stream);
  put32(stream,0x30,QD_WIDTH);put32(stream,0x34,QD_STORED_HEIGHT);
  put32(stream,0x48,QD_WIDTH);put32(stream,0x4c,QD_STORED_HEIGHT);put32(stream,0x2c,103);
  QduRequest small={frame,stream,QD_QUALITY,0};
  int rc=o->encode(encoder,&small);
  if(rc){s->quarantined=owned;s->blocked=1;s->failed++;return rc;}
  uint64_t end=(uint64_t)u32(stream,0x3c)+u32(stream,0x40);
  if(!u32(stream,0x40)||end>u32(r->stream,0x38)){
   if(o->release(o->context,owned)){s->quarantined=owned;s->blocked=1;}
   s->failed++;return -1001;
  }
  memcpy(r->stream,stream,sizeof stream);
  if(o->release(o->context,owned)){s->quarantined=owned;s->blocked=1;}
  s->direct++;return 0;
 }
cleanup:
 /* HAL尚未调用；任何映射清理失败都停止直编，避免重复释放不确定的句柄。 */
 if(src_mapped&&o->unmap(input)){s->blocked=1;}
 if(dst_mapped&&o->unmap(owned)){s->blocked=1;s->quarantined=owned;owned=0;}
 if(owned&&o->release(o->context,owned)){s->blocked=1;s->quarantined=owned;}
fallback:
 s->fallback++;return o->encode(encoder,r);
}
