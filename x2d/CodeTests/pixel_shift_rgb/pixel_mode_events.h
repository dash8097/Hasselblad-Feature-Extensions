/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* Linux inotify 的固定头；有界读取名称，不依赖缓冲对齐。 */
#ifndef PIXEL_MODE_EVENTS_H
#define PIXEL_MODE_EVENTS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static int po_arm_event(const void *buffer,size_t bytes,int *changed){
 const uint8_t *p=buffer;size_t at=0;int seen=0;
 while(at<bytes){
  uint32_t header[4];if(bytes-at<sizeof header)return 0;
  memcpy(header,p+at,sizeof header);uint32_t length=header[3];
  if(length>bytes-at-sizeof header)return 0;
  if(header[1]&0x4000u)seen=1; /* IN_Q_OVERFLOW：重新读当前状态。 */
  if(length){
   const char *name=(const char*)p+at+sizeof header;
   if(!memchr(name,0,length))return 0;
   if(!strcmp(name,"arm.txt"))seen=1;
  }
  at+=sizeof header+length;
 }
 *changed=seen;return 1;
}
#endif
