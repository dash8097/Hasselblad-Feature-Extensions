/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#include <string.h>
#include <stdio.h>
#include "factory_raw_stream.h"
typedef struct {unsigned char bytes[16];uint32_t size,pos,chunk;int error;} Source;
static long read_source(void*ctx,unsigned char*dst,uint32_t n){
 Source*s=ctx;if(s->error)return -1;
 uint32_t left=s->size-s->pos;if(n>left)n=left;if(n>s->chunk)n=s->chunk;
 memcpy(dst,s->bytes+s->pos,n);s->pos+=n;return n;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"failed line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 unsigned char dst[20];Source s={{1,2,3,4,5,6,7,8},8,0,3,0};
 memset(dst,0xa5,sizeof dst);
 CHECK(factory_raw_stream(read_source,&s,dst,8,16));
 CHECK(!memcmp(dst,s.bytes,8));for(unsigned i=8;i<16;i++)CHECK(dst[i]==0);
 for(unsigned i=16;i<20;i++)CHECK(dst[i]==0xa5);
 s.pos=0;s.size=7;CHECK(!factory_raw_stream(read_source,&s,dst,8,16));
 s.pos=0;s.size=9;CHECK(!factory_raw_stream(read_source,&s,dst,8,16));
 s.pos=0;s.size=8;s.error=1;CHECK(!factory_raw_stream(read_source,&s,dst,8,16));
 s.error=0;CHECK(!factory_raw_stream(read_source,&s,dst,8,7));
 CHECK(!factory_raw_stream(read_source,&s,dst,0,16));
 puts("RAW_STREAM_MOCK_PASSED; NO_IMAGE_READ");return 0;
}
