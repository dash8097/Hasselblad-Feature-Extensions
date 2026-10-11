/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 从原厂 YUV422SP 块裁切到合并缓冲。
 * GRBG 输入使用左侧一列重叠对齐原厂 RGGB 输入相位；输出裁掉该列。
 * 奇数列裁切时 U、V 各自做半像素重新采样，不交换或混合颜色分量。
 * 这是候选 ROI 适配，不是原厂可配置 CFA 已验证的声明。 */
#ifndef FACTORY_TILE_COPY_H
#define FACTORY_TILE_COPY_H
#include <stdint.h>
#include <stddef.h>
#include "factory_ion_copy.h"
static int factory_tile_copy(const uint8_t *sy,const uint8_t *suv,
 uint32_t sw,uint32_t sh,uint32_t ss,uint32_t sx,
 uint8_t *dy,uint8_t *duv,uint32_t dw,uint32_t dh,uint32_t ds,
 uint32_t ox,uint32_t oy,uint32_t width,uint32_t height){
 if(!sy||!suv||!dy||!duv||!sw||!sh||!dw||!dh||(sw&1)||(dw&1)||
    ss<sw||ds<dw||(ox&1)||(width&1)||!width||!height||
    ox>=dw||oy>=dh||width>dw-ox||height>dh-oy||height>sh||
    sx>=sw||width>sw-sx||((sx&1)&&width>=sw-sx)||
    (uint64_t)ss*sh>SIZE_MAX||(uint64_t)ds*dh>SIZE_MAX)return 0;
 for(uint32_t y=0;y<height;y++){
  const uint8_t *a=sy+(size_t)y*ss+sx;
  uint8_t *b=dy+(size_t)(oy+y)*ds+ox;
  factory_ion_copy(b,a,width);
  a=suv+(size_t)y*ss+(sx&~1u);
  b=duv+(size_t)(oy+y)*ds+ox;
  uint32_t x=0;
#ifdef __aarch64__
  if(sx&1){
   for(;width-x>=16;x+=16)vst1q_u8(b+x,vrhaddq_u8(vld1q_u8(a+x),vld1q_u8(a+x+2)));
  }else{factory_ion_copy(b,a,width);x=width;}
#endif
  for(;x<width;x+=2){
   if(sx&1){
    b[x]=(uint8_t)(((unsigned)a[x]+a[x+2]+1)/2);
    b[x+1]=(uint8_t)(((unsigned)a[x+1]+a[x+3]+1)/2);
   }else{b[x]=a[x];b[x+1]=a[x+1];}
  }
 }
 return 1;
}
#endif
