/* 官方 4.2.0 libdcam_base 序列化布局；只检查结构，不记录照片参数。
 * 头部四个 uint32，偏移表、数据区、24 字节标签记录。
 * 标签记录 +8/+12 为 val_type/ele_cnt；+16 指针由 deserialize 重建。 */
#ifndef CAPTURE_RAWPB_METADATA_H
#define CAPTURE_RAWPB_METADATA_H
#include <stdint.h>
#include <stddef.h>
static uint32_t crm_u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int crm_layout(const uint8_t *p,size_t n){
 if(!p||n<16||n>16u*1024u*1024u)return 0;
 uint32_t cap=crm_u32(p),used=crm_u32(p+4),tc=crm_u32(p+8),tags=crm_u32(p+12);
 if(!tags||tags>4096||tc<tags||tc>4096||cap<used||cap>16u*1024u*1024u||
    16ull+28ull*tags+used!=n)return 0;
 for(uint32_t i=0;i<tags;i++)if(crm_u32(p+16+4*i)>used)return 0;
 return 1;
}
static int crm_byte_tag(const uint8_t *p,size_t n,uint32_t id,uint32_t length,const uint8_t **out){
 if(!crm_layout(p,n))return 0;
 uint32_t tags=crm_u32(p+12),used=crm_u32(p+4);size_t data=16+4u*tags,records=data+used;
 unsigned found=0;
 for(uint32_t i=0;i<tags;i++){
  const uint8_t *r=p+records+24u*i;
  if(crm_u32(r)!=id)continue;
  uint32_t offset=crm_u32(p+16+4u*i);
  if(found++||crm_u32(r+8)!=1||crm_u32(r+12)!=length||offset>used||length>used-offset)return 0;
  *out=p+data+offset;
 }
 return found==1;
}
static int crm_rawpb_wb_valid(const uint8_t *p,size_t n){
 const uint8_t *wb=0,*neutral=0;
 if(!crm_byte_tag(p,n,0x4d5,24,&wb)||!crm_byte_tag(p,n,0x4d3,16,&neutral))return 0;
 for(unsigned i=0;i<6;i++)if(!crm_u32(wb+4*i)||crm_u32(wb+4*i)>INT32_MAX)return 0;
 for(unsigned i=0;i<4;i++)if(!crm_u32(neutral+4*i))return 0;
 return 1;
}
#endif
