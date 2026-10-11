/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 四亿 JPEG 轻度亮度锐化候选：3x3 二项低通、12.5% 增益、最大三级改变量。
 * 只处理 Y 平面；保留色度、行尾和外边界。使用三行原始样本，内存不随高度增加。
 * 不等同于 Capture One 的 Amount=185/Radius=1/Threshold=1/Halo=0。
 * 用户新增该参考后，本实现仍是待对照候选，不能以参数等价名义部署。 */
#ifndef FACTORY_LIGHT_SHARPEN_H
#define FACTORY_LIGHT_SHARPEN_H
#include <stdint.h>
#include <stddef.h>
static int factory_light_sharpen(uint8_t *y,uint32_t width,uint32_t height,
 uint32_t stride,uint8_t *scratch,size_t scratch_bytes){
 if(!y||!scratch||width<3||height<3||stride<width||scratch_bytes/(size_t)width<3)return 0;
 if((uint64_t)height*stride>SIZE_MAX)return 0;
 for(uint32_t r=0;r<2;r++)for(uint32_t x=0;x<width;x++)scratch[(size_t)r*width+x]=y[(size_t)r*stride+x];
 for(uint32_t r=1;r+1<height;r++){
  uint8_t *above=scratch+(size_t)((r-1)%3)*width;
  uint8_t *center=scratch+(size_t)(r%3)*width;
  uint8_t *below=scratch+(size_t)((r+1)%3)*width;
  for(uint32_t x=0;x<width;x++)below[x]=y[(size_t)(r+1)*stride+x];
  for(uint32_t x=1;x+1<width;x++){
   int blur=above[x-1]+2*above[x]+above[x+1]
           +2*center[x-1]+4*center[x]+2*center[x+1]
           +below[x-1]+2*below[x]+below[x+1];
   int detail=16*center[x]-blur,delta=0;
   /* 约两级亮度差以内不增强；剩余细节按 1/8 增益输出。 */
   if(detail>32)delta=(detail-32+64)/128;
   else if(detail < -32)delta=-((-detail-32+64)/128);
   if(delta>3)delta=3;if(delta < -3)delta=-3;
   int value=center[x]+delta;if(value<0)value=0;if(value>255)value=255;
   y[(size_t)r*stride+x]=(uint8_t)value;
  }
 }
 return 1;
}
#endif
