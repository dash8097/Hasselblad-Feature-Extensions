#ifndef PIXEL_OVERCLOCK_CORE_H
#define PIXEL_OVERCLOCK_CORE_H
#include <stdint.h>
#ifndef PO_EXTERNAL_C_RUNTIME
#include <stdio.h>
#include <string.h>
#endif
typedef struct {unsigned sequence,frames,delay,keep,initial_delay;} PoRequest;
/* 部分写入、过期会话、跳号和重复请求均不能触发曝光。 */
static int po_parse_request(const char *text,size_t size,const char *session,unsigned acknowledged,PoRequest *out){
 if(!text||!session||!out||size<25||size>127||text[size]!=0||text[size-1]!='\n')return 0;
 char version[4]={0},token[9]={0},action[6]={0},end[4]={0};
 unsigned sequence=0,frames=0,delay=0,keep=0,initial_delay=0;int used=0;
 int iso=!memcmp(text,"RS5 ",4);
 int extended=iso||!memcmp(text,"RS4 ",4);
 if(extended){
  if(sscanf(text,"%3s %8s %u %u %u %u %u %5s %3s %n",version,token,&sequence,&frames,&delay,&keep,&initial_delay,action,end,&used)!=9)return 0;
 }else if(sscanf(text,"%3s %8s %u %u %u %u %5s %3s %n",version,token,&sequence,&frames,&delay,&keep,action,end,&used)!=8)return 0;
 int pixel=!strcmp(version,"PO3")&&frames==6&&delay>=2&&delay<=60&&keep<=1;
#ifdef PO_MULTI_RESOLUTION
 /* 25是单张四分之一输出的模式码，不是拍摄25张。 */
 pixel|=!strcmp(version,"PO4")&&((frames==16&&delay>=2&&delay<=60)||(frames==25&&delay==0))&&keep<=1;
#if defined(PO_NATIVE_DISK400) && !defined(PO_DEVELOPMENT_SIXTEEN)
 /* 当前稳定化候选仅开放六张及原有25MP，16张保留源码但不派发。 */
 if(frames==16)pixel=0;
#endif
#endif
#ifdef PO_RAW_STACK
 int stack=((!strcmp(version,"RS3")||!strcmp(version,"RS4"))&&frames<=2&&delay>=1&&delay<=86399&&keep<=1&&!(frames==2&&keep)&&initial_delay<=60)||
  (iso&&frames==3&&delay>=1&&delay<=65535&&keep==0&&initial_delay<=60);
#else
 int stack=0;
#endif
 if((!pixel&&!stack)||strcmp(token,session)||strcmp(action,"START")||strcmp(end,"END")||
    used!=(int)size||sequence>2147483646u||
    acknowledged>=2147483646u||sequence!=acknowledged+1)return 0;
 char canonical[128];int n=extended?
  snprintf(canonical,sizeof canonical,"%s %s %u %u %u %u %u START END\n",version,session,sequence,frames,delay,keep,initial_delay):
  snprintf(canonical,sizeof canonical,"%s %s %u %u %u %u START END\n",version,session,sequence,frames,delay,keep);
 if(n!=(int)size||memcmp(text,canonical,size))return 0;
 *out=(PoRequest){sequence,frames,delay,keep,initial_delay};return 1;
}
#endif
