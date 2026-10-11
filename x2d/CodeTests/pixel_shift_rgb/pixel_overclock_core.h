/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#ifndef PIXEL_OVERCLOCK_CORE_H
#define PIXEL_OVERCLOCK_CORE_H
#include <stdint.h>
#ifndef PO_EXTERNAL_C_RUNTIME
#include <stdio.h>
#include <string.h>
#endif
typedef struct {unsigned sequence,frames,delay,keep;} PoRequest;
/* 部分写入、过期会话、跳号和重复请求均不能触发曝光。 */
static int po_parse_request(const char *text,size_t size,const char *session,unsigned acknowledged,PoRequest *out){
 if(!text||!session||!out||size<25||size>127||text[size]!=0||text[size-1]!='\n')return 0;
 char version[4]={0},token[9]={0},action[6]={0},end[4]={0};
 unsigned sequence=0,frames=0,delay=0,keep=0;int used=0;
 if(sscanf(text,"%3s %8s %u %u %u %u %5s %3s %n",version,token,&sequence,&frames,&delay,&keep,action,end,&used)!=8)
  return 0;
 if(strcmp(version,"PO3")||strcmp(token,session)||strcmp(action,"START")||strcmp(end,"END")||
    used!=(int)size||frames!=6||delay<2||delay>60||keep>1||sequence>2147483646u||
    acknowledged>=2147483646u||sequence!=acknowledged+1)return 0;
 char canonical[128];int n=snprintf(canonical,sizeof canonical,"PO3 %s %u %u %u %u START END\n",session,sequence,frames,delay,keep);
 if(n!=(int)size||memcmp(text,canonical,size))return 0;
 *out=(PoRequest){sequence,frames,delay,keep};return 1;
}
#endif
