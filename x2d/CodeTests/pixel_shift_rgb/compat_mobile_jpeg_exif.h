/* Android Phocus 1.0.2 JPEG 识别所需的最小 MakerNote。
 * 只复制本次 RAW 的传感器类型和机身型号编号，不复制身份、校正或多帧数据。 */
#ifndef COMPAT_MOBILE_JPEG_EXIF_H
#define COMPAT_MOBILE_JPEG_EXIF_H
#define factory_native_exif factory_native_exif_base
#include "factory_native_exif.h"
#undef factory_native_exif

/* 源 RAW 的 MakerNote 可超过通用 EXIF 单字段 4096 字节限制。
 * 只允许其完整范围位于传入头中，随后按 IFD 解析，不复制整块。 */
static int mobile_maker_ifd(const uint8_t *source,size_t bytes,uint32_t exif,
 FactoryExifTag *maker,unsigned *count){
 if(exif>bytes||bytes-exif<6)return 0;
 unsigned n=fe16(source+exif);
 if(n>128||(uint64_t)exif+6+12*n>bytes)return 0;
 for(unsigned i=0;i<n;i++){
  const uint8_t *p=source+exif+2+12*i;
  if(fe16(p)!=37500)continue;
  uint32_t size=fe32(p+4),offset=fe32(p+8);
  if(fe16(p+2)!=7||size<6||(uint64_t)offset+size>bytes)return 0;
  return fe_ifd(source,(size_t)offset+size,offset,maker,count);
 }
 return 0;
}

static int factory_native_exif(const uint8_t *source,size_t bytes,
 uint32_t width,uint32_t height,uint8_t *out,size_t capacity,uint32_t *length){
 uint8_t base[65536],maker[30]={0};uint32_t base_length=0;
 FactoryExifTag root_in[128],exif_in[128],maker_in[128],root_out[128],exif_out[128],ids[2];
 unsigned rn=0,en=0,mn=0,ron=0,eon=0;
 if(!out||!length||!factory_native_exif_base(source,bytes,width,height,base,sizeof base,&base_length))return 0;
 if(!fe_ifd(source,bytes,fe32(source+4),root_in,&rn))return 0;
 const FactoryExifTag *t=fe_find(root_in,rn,34665);
 if(!t||t->type!=4||t->count!=1||!fe_ifd(source,bytes,fe32(t->value),exif_in,&en))return 0;
 if(!mobile_maker_ifd(source,bytes,fe32(t->value),maker_in,&mn))return 0;
 for(unsigned i=0;i<2;i++){
  t=fe_find(maker_in,mn,(uint16_t)(0x11+i));
  if(!t||t->type!=4||t->count!=1||!fe32(t->value))return 0;
  ids[i]=*t;
 }
 uint32_t maker_end=sizeof maker;
 if(!fe_emit(maker,sizeof maker,0,ids,2,&maker_end)||maker_end!=sizeof maker)return 0;
 if(!fe_ifd(base+10,base_length-10,8,root_out,&ron)||ron!=4||
    !fe_ifd(base+10,base_length-10,62,exif_out,&eon)||eon>=128||fe_find(exif_out,eon,37500))return 0;
 exif_out[eon++]=(FactoryExifTag){37500,7,sizeof maker,maker,sizeof maker};
 for(unsigned i=0;i<eon;i++)for(unsigned j=i+1;j<eon;j++)if(exif_out[j].id<exif_out[i].id){
  FactoryExifTag swap=exif_out[i];exif_out[i]=exif_out[j];exif_out[j]=swap;
 }
 uint32_t end=62+6+12*eon;
 if(capacity<10u+end)return 0;
 memset(out,0,capacity);memcpy(out+4,"Exif\0\0II\052\000",10);fep32(out+14,8);
 if(!fe_emit(out+10,capacity-10,8,root_out,ron,&end)||
    !fe_emit(out+10,capacity-10,62,exif_out,eon,&end)||end+6>65533)return 0;
 out[0]=255;out[1]=225;out[2]=(uint8_t)((end+8)>>8);out[3]=(uint8_t)(end+8);
 *length=end+10;return 1;
}
#endif
