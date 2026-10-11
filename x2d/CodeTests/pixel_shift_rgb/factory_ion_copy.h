/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 按块访问原厂映射缓冲，避免逐字节访问非缓存 ION 映射。
 * 只复制已校验的范围，最后不足一个向量的尾部仍按原长度处理。 */
#ifndef FACTORY_ION_COPY_H
#define FACTORY_ION_COPY_H
#include <stdint.h>
#include <stddef.h>
#ifdef __aarch64__
#include <arm_neon.h>
#endif
static inline void factory_ion_copy(uint8_t *d,const uint8_t *s,size_t n){
 size_t p=0;
#ifdef __aarch64__
 for(;n-p>=16;p+=16)vst1q_u8(d+p,vld1q_u8(s+p));
#endif
 for(;p<n;p++)d[p]=s[p];
}
static inline void factory_ion_fill_pair(uint16_t *d,uint16_t a,uint16_t b,size_t n){
 size_t p=0;
#ifdef __aarch64__
 const uint16_t pair[8]={a,b,a,b,a,b,a,b};
 uint16x8_t v=vld1q_u16(pair);
 for(;n-p>=8;p+=8)vst1q_u16(d+p,v);
#endif
 for(;p<n;p++)d[p]=(p&1)?b:a;
}
#endif
