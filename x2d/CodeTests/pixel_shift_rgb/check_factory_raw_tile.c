/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#include <stdio.h>
#include <string.h>
#include "factory_raw_tile.h"
typedef struct {uint16_t image[60];unsigned reads;int fail;} Fixture;
static int read_at(void*ctx,uint64_t offset,unsigned char*dst,uint32_t n){
 Fixture*f=ctx;f->reads++;if(f->fail||offset+n>sizeof f->image)return 0;
 memcpy(dst,(unsigned char*)f->image+offset,n);return 1;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"failed line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 Fixture f={0};uint16_t out[40],a[6],b[6];
 for(unsigned y=0;y<6;y++)for(unsigned x=0;x<10;x++)f.image[y*10+x]=(uint16_t)(y*100+x);
 memset(out,0xa5,sizeof out);
 CHECK(factory_raw_tile(read_at,&f,out,8,6,10,6,4,8,0,0,2,2,a,b));
 const uint16_t first[]={0,1,0,1,2,3,0,0,100,101,100,101,102,103,0,0,
                         0,1,0,1,2,3,0,0,100,101,100,101,102,103,0,0};
 CHECK(!memcmp(out,first,sizeof first));CHECK(f.reads==2);
 for(unsigned i=32;i<40;i++)CHECK(out[i]==0xa5a5);
 f.reads=0;
 CHECK(factory_raw_tile(read_at,&f,out,8,6,10,6,4,8,6,4,2,0,a,b));
 const uint16_t last[]={404,405,406,407,406,407,0,0,504,505,506,507,506,507,0,0,
                        404,405,406,407,406,407,0,0,504,505,506,507,506,507,0,0};
 CHECK(!memcmp(out,last,sizeof last));CHECK(f.reads==2);
 f.fail=1;CHECK(!factory_raw_tile(read_at,&f,out,8,6,10,6,4,8,0,0,2,2,a,b));
 CHECK(!factory_raw_tile(read_at,&f,out,7,6,10,6,4,8,0,0,2,2,a,b));
 uint64_t pixels=0;unsigned count=0;
 for(unsigned y=0;y<17498;y+=8742)for(unsigned x=0;x<23326;x+=11656){
  unsigned w=23326-x,h=17498-y;if(w>11656)w=11656;if(h>8742)h=8742;
  CHECK(!(w&1)&&!(h&1));pixels+=(uint64_t)w*h;count++;
 }
 CHECK(count==9&&pixels==408158348);
 /* 六合一 GRBG 不能直接当作原厂 RGGB：奇数列左侧重叠须翻转红／绿相位。
  * 不同色样值让错误的偶数裁切实际失败，而非只检查编译开关。 */
 static const uint16_t grbg[2][2]={{20,10},{30,20}};
 static const uint16_t rggb[2][2]={{10,20},{20,30}};
 for(unsigned y=0;y<6;y++)for(unsigned x=0;x<10;x++)f.image[y*10+x]=grbg[y&1][x&1];
 f.fail=0;
 CHECK(factory_raw_tile(read_at,&f,out,8,6,10,6,4,8,0,0,3,2,a,b));
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<6;x++)CHECK(out[y*8+x]==rggb[y&1][x&1]);
 CHECK(factory_raw_tile(read_at,&f,out,8,6,10,6,4,8,0,0,2,2,a,b));
 CHECK(out[0]!=rggb[0][0]);
 pixels=0;count=0;
 for(unsigned y=0;y<17498;y+=8742)for(unsigned x=0;x<23326;x+=11654){
  unsigned w=23326-x,h=17498-y;if(w>11654)w=11654;if(h>8742)h=8742;
  CHECK(!(x&1)&&!(w&1)&&!(h&1));pixels+=(uint64_t)w*h;count++;
 }
 CHECK(count==9&&pixels==408158348);
 puts("RAW_TILE_CFA_EDGES_SHORT_READ_AND_FULL_COVERAGE_PASSED");return 0;
}
