/* 官方4.2.0 ImageMemory::buildDesc / DussMemShare::buildDesc字段。
 * 仅从已知一亿完整缓冲重建几何；未知/重复/缺失字段拒绝。
 * 不包含元数据修改、分配或设备调用。 */
#ifndef FACTORY_DESCRIPTOR_REWRITE_H
#define FACTORY_DESCRIPTOR_REWRITE_H
#include "factory_buffer_geometry.h"
typedef struct {
 FactoryBufferGeometry before,after;
 uint64_t new_handle,seen;
} FactoryDescriptorRewrite;
static const char *const factory_descriptor_keys[]={
 "WIDTH","HEIGHT","STRIDE","FORMAT","SRC_FORMAT","CROP_X","CROP_Y","CROP_W","CROP_H",
 "DATA_SIZE","REQUESTED_SIZE",
 "PLANE_0_WIDTH","PLANE_0_HEIGHT","PLANE_0_OFFSET","PLANE_0_CONTENT_SIZE",
 "PLANE_1_WIDTH","PLANE_1_HEIGHT","PLANE_1_OFFSET","PLANE_1_CONTENT_SIZE",
 "SHARE_HANDLE","DUSS_OFFSET","DESCRIPTION"
};
static int factory_key_equal(const char*a,const char*b) {
 if(!a||!b)return 0;
 while(*a&&*a==*b){a++;b++;}return *a==*b;
}
static uint64_t factory_descriptor_mask(unsigned planes) {
 uint64_t mask=(UINT64_C(1)<<22)-1;
 if(planes==1)mask&=~(UINT64_C(15)<<15);
 return mask;
}
static uint32_t factory_geometry_field(const FactoryBufferGeometry*g,unsigned field) {
 switch(field) {
 case 0:return g->width;case 1:return g->height;case 2:return g->stride_pixels;
 case 3:return g->format;case 4:return g->source_format;
 case 5:return g->crop_x;case 6:return g->crop_y;case 7:return g->crop_w;case 8:return g->crop_h;
 case 9:case 10:return g->allocation_bytes;
 default:{const FactoryPlaneGeometry*p=&g->planes[(field-11)/4];
  switch((field-11)%4){case 0:return p->width;case 1:return p->height;case 2:return p->offset;default:return p->content_size;}}
 }
}
static int factory_descriptor_begin(FactoryDescriptorRewrite*s,unsigned side,
 const FactoryBufferGeometry*after,uint64_t allocated,uint64_t handle) {
 if(!s||side>1||!handle||!factory_buffer_fits(after,allocated))return 0;
 FactoryDescriptorRewrite next={0};
 if(!factory_buffer_geometry(&next.before,side?11656:11836,side?8742:8842,side?6:1))return 0;
 if(after->format!=next.before.format)return 0;
 next.after=*after;next.new_handle=handle;*s=next;return 1;
}
/* 返回1表示字符串原样复制，2表示使用替换数值，0拒绝。
 * type使用D-Bus类型字符，numeric由调用方按真实类型读取。 */
static int factory_descriptor_field(FactoryDescriptorRewrite*s,const char*key,
 int type,uint64_t numeric,uint64_t*replacement) {
 if(!s||!replacement)return 0;
 unsigned i=0;for(;i<22;i++)if(factory_key_equal(key,factory_descriptor_keys[i]))break;
 if(i==22||!(factory_descriptor_mask(s->before.plane_count)&(UINT64_C(1)<<i))||
    (s->seen&(UINT64_C(1)<<i)))return 0;
 uint64_t value=0;int result=2;
 if(i==21){if(type!='s')return 0;result=1;}
 else if(i==19){if(type!='t'||!numeric)return 0;value=s->new_handle;}
 else if(i==20){if(type!='t'||numeric)return 0;value=0;}
 else {
  if(type!=(i==9||i==10?'u':'i')||numeric!=factory_geometry_field(&s->before,i))return 0;
  value=factory_geometry_field(&s->after,i);
  if(type=='i'&&value>INT32_MAX)return 0;
 }
 s->seen|=UINT64_C(1)<<i;*replacement=value;return result;
}
static int factory_descriptor_complete(const FactoryDescriptorRewrite*s) {
 return s&&s->seen==factory_descriptor_mask(s->before.plane_count);
}
#endif
