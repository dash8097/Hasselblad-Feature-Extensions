/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 项目自写的进程租约检查：核对进程身份、期限及调用者提供的资源标识。
 * 本文件没有共享内存导入、厂商符号绑定或实际显影调用。 */
#ifndef X2D_PIXEL_RENDER_ROUTE_H
#define X2D_PIXEL_RENDER_ROUTE_H
#include <stdint.h>
#include <stddef.h>
#define PO_RENDER_MAGIC UINT64_C(0x3254523430443258)
#define PO_RENDER_LEASE_MS 30000u
typedef struct {
 uint64_t magic;
 uint32_t pid,reserved;
 uint64_t start_ticks,until_ms,input,output;
} PoRenderLease;
_Static_assert(sizeof(PoRenderLease)==48,"lease ABI");
static inline int po_render_start_ticks(const char *s,size_t n,uint64_t *out){
 size_t p=n;while(p&&s[p-1]!=')')p--;if(!p||p+1>=n||s[p]!=' ')return 0;
 p++;
 for(unsigned field=3;field<22;field++){
  if(p>=n||s[p]==' '||s[p]=='\n')return 0;
  while(p<n&&s[p]!=' '&&s[p]!='\n')p++;
  if(p>=n||s[p]!=' ')return 0;p++;
 }
 uint64_t v=0;size_t first=p;
 while(p<n&&s[p]>='0'&&s[p]<='9'){
  unsigned d=(unsigned)(s[p++]-'0');if(v>(UINT64_MAX-d)/10)return 0;v=v*10+d;
 }
 if(p==first||p>=n||s[p]!=' '||!v)return 0;*out=v;return 1;
}
static inline int po_render_lease_valid(const PoRenderLease *l,size_t n,uint64_t t,
                               int identity_ok,uint64_t ticks){
 return n==sizeof *l&&l->magic==PO_RENDER_MAGIC&&!l->reserved&&l->pid>1&&
   identity_ok&&ticks==l->start_ticks&&l->start_ticks&&t&&l->until_ms>t&&
   l->until_ms-t<=PO_RENDER_LEASE_MS&&l->input&&l->output&&l->input!=l->output;
}
static inline unsigned po_render_group(const PoRenderLease *l,unsigned group,
                                uint64_t input,uint64_t output){
 return group==12&&input==l->input&&output==l->output?11:group;
}
#endif
