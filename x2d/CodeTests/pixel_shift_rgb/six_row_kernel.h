/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 六合一的无符号 16 位数值域逐行计算；文件与内存路径共用，保持偶数舍入与 GRBG。
 * 输入每行四个整数位移通道，以及两帧半像素采样的三通道/有效位。 */
#ifndef SIX_ROW_KERNEL_H
#define SIX_ROW_KERNEL_H
#include <stdint.h>
#include <stdlib.h>
static inline uint16_t six_native_mean(unsigned sum,unsigned count){
 unsigned quotient,remainder;
 if(count==2){quotient=sum>>1;remainder=sum&1;}
 else if(count==3){quotient=sum/3;remainder=sum-quotient*3;}
 else if(count==4){quotient=sum>>2;remainder=sum&3;}
 else abort();
 if(remainder*2>count||(remainder*2==count&&(quotient&1)))quotient++;
 return (uint16_t)quotient;
}
typedef struct {
 unsigned width;
 const uint16_t (*a)[4],(*b)[4],(*cur)[3],(*prev)[3];
 const uint8_t *cur_valid,*prev_valid;
 uint16_t *output;
} SixNativeRow;
static inline void six_native_range(unsigned begin,unsigned end,void *context){
 SixNativeRow *c=context;
 for(unsigned x=begin;x<end;x++){
  unsigned next=x+1<c->width?x+1:x,previous=x?x-1:0;
  c->output[2*x]=(uint16_t)(((unsigned)c->a[x][1]+c->a[x][3]+1)/2);
  c->output[2*c->width+2*x+1]=c->cur[x][1];
  unsigned sum=(unsigned)c->a[x][0]+c->a[next][0],count=2;
  if(c->prev_valid[x]&1){sum+=c->prev[x][0];count++;}
  if(c->cur_valid[x]&1){sum+=c->cur[x][0];count++;}
  c->output[2*x+1]=six_native_mean(sum,count);
  sum=(unsigned)c->a[x][2]+c->b[x][2];count=2;
  if(c->cur_valid[x]&4){sum+=c->cur[x][2];count++;}
  if(c->cur_valid[previous]&4){sum+=c->cur[previous][2];count++;}
  c->output[2*c->width+2*x]=six_native_mean(sum,count);
 }
}
#endif
