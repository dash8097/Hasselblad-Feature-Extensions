/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 默认预览的 YUV422 区域平均。只缩小；两个平面分别保持 Y/UV 语义。
 * 输入为原厂显影的全幅 YUV，不重新解拜尔、不另作颜色处理。
 * 这不是原厂缩放算法等价声明；编码及机内显示须另行验证。 */
#ifndef FACTORY_PREVIEW_SCALE_H
#define FACTORY_PREVIEW_SCALE_H
#include <stdint.h>
#include <stddef.h>
#include "factory_ion_copy.h"
static int factory_preview_scale(const uint8_t *source,
 uint32_t sw,uint32_t sh,uint32_t ss,uint8_t *target,
 uint32_t tw,uint32_t th,uint32_t ts){
 if(!source||!target||source==target||sw<2||sh<2||tw<2||th<2||
    sw>32768||sh>32768||(sw&1)||(tw&1)||tw>sw||th>sh||ss<sw||ts<tw||
    (uint64_t)ss*sh*2>SIZE_MAX||(uint64_t)ts*th*2>SIZE_MAX)return 0;
 /* 先批量读取一行到缓存内存，再累计列和。保持原有区域平均及四舍五入，
  * 不改色彩或滤波；每个输入字节只从 ION 读取一次。空间上界小于 300 KiB。 */
 uint8_t row[32768];uint32_t sums[32768],edges[32769];
 for(unsigned plane=0;plane<2;plane++){
  const uint8_t *src=source+(size_t)ss*sh*plane;
  uint8_t *dst=target+(size_t)ts*th*plane;
  unsigned components=plane?2:1,swidth=sw/components,twidth=tw/components;
  for(uint32_t x=0;x<=twidth;x++)edges[x]=(uint32_t)((uint64_t)x*swidth/twidth);
  for(uint32_t y=0;y<th;y++){
   uint32_t top=(uint32_t)((uint64_t)y*sh/th);
   uint32_t bottom=(uint32_t)((uint64_t)(y+1)*sh/th);
   for(uint32_t x=0;x<sw;x++)sums[x]=0;
   for(uint32_t sy=top;sy<bottom;sy++){
    factory_ion_copy(row,src+(size_t)sy*ss,sw);
    for(uint32_t x=0;x<sw;x++)sums[x]+=row[x];
   }
   for(uint32_t x=0;x<twidth;x++){
    uint32_t left=edges[x],right=edges[x+1];
    uint64_t sum[2]={0,0};uint32_t count=(right-left)*(bottom-top);
    for(uint32_t sx=left;sx<right;sx++)
     for(unsigned c=0;c<components;c++)sum[c]+=sums[sx*components+c];
    for(unsigned c=0;c<components;c++)
     dst[(size_t)y*ts+x*components+c]=(uint8_t)((sum[c]+count/2)/count);
   }
   for(uint32_t x=tw;x<ts;x++)dst[(size_t)y*ts+x]=0;
  }
 }
 return 1;
}
#endif
