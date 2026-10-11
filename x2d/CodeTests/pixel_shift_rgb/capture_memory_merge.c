/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#include "capture_memory_merge.h"
#include "six_row_kernel.h"
size_t cm_merge_scratch_bytes(unsigned width){return width&&width<=11663?(size_t)width*30:0;}
static const uint16_t *source_row(const CmView *frame,unsigned y){
 return (const uint16_t*)(frame->data+(size_t)y*frame->stride);
}
static void integer_row(const CmView frames[6],unsigned width,unsigned y,uint16_t dst[][4]){
 static const unsigned cfa[2][2]={{0,1},{3,2}},dx[4]={0,0,1,1},dy[4]={0,1,0,1};
 for(unsigned i=0;i<4;i++){
  unsigned sy=y+1-dy[i];const uint16_t *src=source_row(frames+i,sy+92);
  for(unsigned x=0;x<width;x++)dst[x][cfa[sy%2][(x+dx[i])%2]]=src[124+dx[i]+x];
 }
}
static int half_row(const CmView frames[6],unsigned width,unsigned y,uint16_t dst[][3],uint8_t *valid){
 static const unsigned cfa[2][2]={{0,1},{3,2}};
 memset(dst,0,(size_t)width*6);memset(valid,0,width);
 for(unsigned i=4;i<6;i++){
  unsigned sy=y+(i==4?1:0);const uint16_t *src=source_row(frames+i,sy+92);
  for(unsigned x=0;x<width;x++){
   unsigned c=cfa[sy%2][(x+1)%2];if(c==3)c=1;
   if(valid[x]&(1u<<c))return 0;dst[x][c]=src[125+x];valid[x]|=(uint8_t)(1u<<c);
  }
 }
 for(unsigned x=0;x<width;x++)if(valid[x]!=3&&valid[x]!=6)return 0;
 return 1;
}
int cm_merge_rows(const CmView frames[6],unsigned width,unsigned height,
 unsigned first,unsigned rows,uint16_t *output,size_t output_bytes,
 void *scratch,size_t scratch_bytes,CmMergeDispatch dispatch,void *context){
 size_t needed=cm_merge_scratch_bytes(width);
 if(!frames||!needed||!height||height>8749||!rows||rows>1024||first>=height||rows>height-first||
    !output||!scratch||((uintptr_t)output&1)||((uintptr_t)scratch&1)||scratch_bytes<needed||
    output_bytes!=(size_t)width*rows*8)return 0;
 /* 无输出写入前核对全部输入；不扣黑电平、不改变原厂数值域。 */
 for(unsigned i=0;i<6;i++){
  const CmView *v=frames+i;
  if(v->width!=11836||v->height!=8842||v->format!=1||v->stride!=23808||
     v->owns_allocation||!v->data||((uintptr_t)v->data&1)||v->bytes<210510336)return 0;
 }
 uint16_t (*a[2])[4],(*half[2])[3];uint8_t *valid[2];uint8_t *p=scratch;
 for(unsigned i=0;i<2;i++){a[i]=(uint16_t(*)[4])p;p+=(size_t)width*8;}
 for(unsigned i=0;i<2;i++){half[i]=(uint16_t(*)[3])p;p+=(size_t)width*6;}
 valid[0]=p;valid[1]=p+width;
 integer_row(frames,width,first,a[0]);
 if(!half_row(frames,width,first?first-1:0,half[1],valid[1]))return 0;
 for(unsigned n=0;n<rows;n++){
  unsigned y=first+n,current=n%2,previous=1-current;
  integer_row(frames,width,y+1<height?y+1:y,a[previous]);
  if(!half_row(frames,width,y,half[current],valid[current]))return 0;
  SixNativeRow row={width,(const uint16_t(*)[4])a[current],(const uint16_t(*)[4])a[previous],
   (const uint16_t(*)[3])half[current],(const uint16_t(*)[3])half[previous],
   valid[current],valid[previous],output+(size_t)n*width*4};
  if(dispatch)dispatch(context,width,six_native_range,&row);else six_native_range(0,width,&row);
 }
 return 1;
}
