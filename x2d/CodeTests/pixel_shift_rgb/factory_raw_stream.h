/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 逐块读取已准备的、带行填充的RAW16，不解析3FR/DNG，不转换黑电平。
 * 回调返回正字节数、0=EOF、负数=失败；接收长度必须精确匹配。
 * 传输逻辑与系统调用分开，便于离线验证短读/截断/多余尾部。 */
#ifndef FACTORY_RAW_STREAM_H
#define FACTORY_RAW_STREAM_H
#include <stdint.h>
typedef long (*FactoryRawRead)(void*,unsigned char*,uint32_t);
static int factory_raw_stream(FactoryRawRead read,void*ctx,unsigned char*dst,
 uint32_t content,uint32_t allocated) {
 if(!read||!dst||!content||allocated<content)return 0;
 uint32_t done=0;
 while(done<content){
  uint32_t chunk=content-done;if(chunk>1048576)chunk=1048576;
  long n=read(ctx,dst+done,chunk);
  if(n<=0||(uint64_t)n>chunk)return 0;
  done+=(uint32_t)n;
 }
 unsigned char tail;
 if(read(ctx,&tail,1)!=0)return 0;
 for(uint32_t i=content;i<allocated;i++)dst[i]=0;
 return 1;
}
#endif
