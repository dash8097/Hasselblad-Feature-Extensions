/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 只读合成容器：核验固定几何后直接按紧密行读取，省去整文件填充复制。 */
#ifndef FACTORY_PACKED_DNG_H
#define FACTORY_PACKED_DNG_H
#include <stdint.h>
static uint32_t packed_u16(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8);}
static uint32_t packed_u32(const uint8_t *p){return packed_u16(p)|(packed_u16(p+2)<<16);}
static const uint8_t *packed_tag(const uint8_t *h,uint32_t directory,uint32_t tag){
 uint32_t count=packed_u16(h+directory);const uint8_t *result=0;
 for(uint32_t i=0;i<count;i++){
  const uint8_t *p=h+directory+2+12*i;
  if(packed_u16(p)==tag){if(result)return 0;result=p;}
 }
 return result;
}
static int factory_packed_dng_geometry(const uint8_t *h,uint64_t bytes,int six,
                                     uint32_t *stride,uint32_t *base){
 if(!h||!stride||!base||bytes!=816320792ULL||h[0]!='I'||h[1]!='I'||h[2]!=42||h[3])return 0;
 uint32_t directory=packed_u32(h+4);
 if(directory<8||directory>4090)return 0;
 uint32_t count=packed_u16(h+directory);
 if(!count||count>128||directory+2+12*count+4>4096)return 0;
 const uint32_t tags[]={256,257,258,259,262,277,273,279,50717};
 const uint32_t values[]={23326,17498,16,1,32803,1,4096,816316696,65535};
 for(unsigned i=0;i<9;i++){
  const uint8_t *p=packed_tag(h,directory,tags[i]);if(!p||packed_u32(p+4)!=1)return 0;
  uint32_t type=packed_u16(p+2),value;
  if(type==3)value=packed_u16(p+8);else if(type==4)value=packed_u32(p+8);else return 0;
  if(value!=values[i])return 0;
 }
 const uint8_t *cfa=packed_tag(h,directory,33422);
 const uint8_t four[]={0,1,1,2},six_phase[]={1,0,2,1};
 if(!cfa||packed_u16(cfa+2)!=1||packed_u32(cfa+4)!=4)return 0;
 for(unsigned i=0;i<4;i++)if(cfa[8+i]!=(six?six_phase[i]:four[i]))return 0;
 *stride=23326;*base=4096;return 1;
}
#endif
