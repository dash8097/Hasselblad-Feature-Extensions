/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 从完整填充RAW读取原尺寸显影块；边缘复制保持RGGB奇偶相位。 */
#ifndef FACTORY_RAW_TILE_H
#define FACTORY_RAW_TILE_H
#include <stdint.h>
#include "factory_ion_copy.h"
typedef int (*FactoryReadAt)(void*,uint64_t,unsigned char*,uint32_t);
static uint32_t factory_cfa_edge(int64_t coordinate,uint32_t size){
 if(coordinate<0)return (uint32_t)((uint64_t)coordinate&1);
 if((uint64_t)coordinate>=size)return size-2+((uint32_t)coordinate&1);
 return (uint32_t)coordinate;
}
static int factory_raw_tile(FactoryReadAt read_at,void*context,uint16_t*target,
 uint32_t sw,uint32_t sh,uint32_t source_stride,uint32_t tw,uint32_t th,uint32_t stride,
 uint32_t ox,uint32_t oy,uint32_t crop_x,uint32_t crop_y,uint16_t*cache0,uint16_t*cache1){
 if(!read_at||!target||!cache0||!cache1||sw<2||sh<2||(sw&1)||(sh&1)||
    source_stride<sw||!tw||!th||stride<tw||ox>=sw||oy>=sh||crop_x>=tw||crop_y>=th)return 0;
 int64_t left=(int64_t)ox-crop_x,right=left+tw;
 uint32_t first=left<0?0:(uint32_t)left,last=right>sw?sw:(uint32_t)right;
 /* 两个边缘相位都必须在读取区间内。 */
 if(first>=last||last-first<2)return 0;
 uint32_t cached_y[2]={UINT32_MAX,UINT32_MAX};uint16_t*cache[2]={cache0,cache1};
 for(uint32_t y=0;y<th;y++){
  uint32_t sy=factory_cfa_edge((int64_t)oy+y-crop_y,sh),slot=sy&1;
  if(cached_y[slot]!=sy){
   uint64_t offset=((uint64_t)sy*source_stride+first)*2;
   if(!read_at(context,offset,(unsigned char*)cache[slot],(last-first)*2))return 0;
   cached_y[slot]=sy;
  }
  uint16_t*row=target+(uint64_t)y*stride;
  uint32_t begin=(uint32_t)((int64_t)first-left),end=begin+last-first;
  if(begin>tw||end>tw)return 0;
  if(begin){
   uint32_t a=factory_cfa_edge(left,sw),b=factory_cfa_edge(left+1,sw);
   if(a<first||a>=last||b<first||b>=last)return 0;
   factory_ion_fill_pair(row,cache[slot][a-first],cache[slot][b-first],begin);
  }
  factory_ion_copy((uint8_t*)(row+begin),(const uint8_t*)cache[slot],(last-first)*2);
  if(end<tw){
   uint32_t a=factory_cfa_edge(left+end,sw),b=factory_cfa_edge(left+end+1,sw);
   if(a<first||a>=last||b<first||b>=last)return 0;
   factory_ion_fill_pair(row+end,cache[slot][a-first],cache[slot][b-first],tw-end);
  }
  factory_ion_fill_pair(row+tw,0,0,stride-tw);
 }
 return 1;
}
#endif
