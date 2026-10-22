#include "capture_memory_merge.h"
#include "six_row_kernel.h"
size_t cm_merge_scratch_bytes(unsigned width){return width&&width<=11663?(size_t)width*32:0;}
static const uint16_t *source_row(const CmView *frame,unsigned y,unsigned row0){
 return (const uint16_t*)(frame->data+(size_t)(y-row0)*frame->stride);
}
/* 原厂借用图像缓冲不保证 CPU 缓存属性。顺序搬入一行，再在普通内存中拆通道；
 * 不改变原厂映射、同步或引用，不另存整幅源图。 */
static const uint16_t *read_samples(const uint16_t *source,unsigned width,uint16_t *line){
#ifdef CM_DIRECT_SOURCE_BENCHMARK
 (void)width;(void)line;return source;
#else
 if(!line)return source;
 memcpy(line,source,(size_t)width*2);return line;
#endif
}
static void integer_row(const CmView frames[6],unsigned width,unsigned y,uint16_t dst[][4],uint16_t *line,unsigned row0){
 static const unsigned cfa[2][2]={{0,1},{3,2}},dx[4]={0,0,1,1},dy[4]={0,1,0,1};
 for(unsigned i=0;i<4;i++){
  unsigned sy=y+1-dy[i];const uint16_t *src=source_row(frames+i,sy+92,row0);
  src=read_samples(src+124+dx[i],width,line);
  for(unsigned x=0;x<width;x++)dst[x][cfa[sy%2][(x+dx[i])%2]]=src[x];
 }
}
static void half_row(const CmView frames[6],unsigned width,unsigned y,uint16_t dst[][3],uint8_t *valid,uint16_t *line,unsigned row0){
 static const unsigned cfa[2][2]={{0,1},{3,2}};
 memset(dst,0,(size_t)width*6);
 /* 两个固定半像素相位使用相邻奇偶行：偶数列必为 GB，奇数列必为 RG。
  * 有效位来自格式规则，不检查像素内容；输入格式在批次入口核对一次。 */
 for(unsigned x=0;x<width;x++)valid[x]=(x&1)?3:6;
 for(unsigned i=4;i<6;i++){
  unsigned sy=y+(i==4?1:0);const uint16_t *src=source_row(frames+i,sy+92,row0);
  src=read_samples(src+125,width,line);
  for(unsigned x=0;x<width;x++){
   unsigned c=cfa[sy%2][(x+1)%2];if(c==3)c=1;
   dst[x][c]=src[x];
  }
 }
}
static int merge_rows(const CmView frames[6],unsigned row0,unsigned span,int cached,unsigned width,unsigned height,
 unsigned first,unsigned rows,uint16_t *output,size_t output_bytes,
 void *scratch,size_t scratch_bytes,CmMergeDispatch dispatch,void *context){
 size_t needed=cm_merge_scratch_bytes(width);
 if(!frames||!needed||!height||height>8749||!rows||rows>1024||first>=height||rows>height-first||
    !output||!scratch||((uintptr_t)output&1)||((uintptr_t)scratch&1)||scratch_bytes<needed||
    output_bytes!=(size_t)width*rows*8)return 0;
 /* 无输出写入前核对全部输入；不扣黑电平、不改变原厂数值域。 */
 unsigned needed_first=first?first+91:92;
 unsigned needed_last=(first+rows<height?first+rows:height-1)+93;
 if(row0>needed_first||!span||row0>=8842||span>8842-row0||needed_last>=row0+span)return 0;
 for(unsigned i=0;i<6;i++){
  const CmView *v=frames+i;
  if(!cached&&x2d_capture_raw_geometry(v->width,v->height,v->format,v->stride)!=X2D_CAPTURE_RAW_VALID)return 0;
  if(v->stride<(width+126)*2||v->stride>23808||(v->stride&1)||
     v->owns_allocation||!v->data||((uintptr_t)v->data&1)||v->bytes<(size_t)span*v->stride)return 0;
 }
 uint16_t (*a[2])[4],(*half[2])[3];uint8_t *valid[2];uint8_t *p=scratch;
 for(unsigned i=0;i<2;i++){a[i]=(uint16_t(*)[4])p;p+=(size_t)width*8;}
 for(unsigned i=0;i<2;i++){half[i]=(uint16_t(*)[3])p;p+=(size_t)width*6;}
 valid[0]=p;valid[1]=p+width;
 uint16_t *line=cached?NULL:(uint16_t *)(p+(size_t)width*2);
 integer_row(frames,width,first,a[0],line,row0);
 half_row(frames,width,first?first-1:0,half[1],valid[1],line,row0);
 for(unsigned n=0;n<rows;n++){
  unsigned y=first+n,current=n%2,previous=1-current;
  integer_row(frames,width,y+1<height?y+1:y,a[previous],line,row0);
  half_row(frames,width,y,half[current],valid[current],line,row0);
  SixNativeRow row={width,(const uint16_t(*)[4])a[current],(const uint16_t(*)[4])a[previous],
   (const uint16_t(*)[3])half[current],(const uint16_t(*)[3])half[previous],
   valid[current],valid[previous],output+(size_t)n*width*4};
  if(dispatch)dispatch(context,width,six_native_range,&row);else six_native_range(0,width,&row);
 }
 return 1;
}
int cm_merge_rows(const CmView frames[6],unsigned width,unsigned height,unsigned first,unsigned rows,
 uint16_t *output,size_t bytes,void *scratch,size_t scratch_bytes,CmMergeDispatch dispatch,void *context){
 return merge_rows(frames,0,8842,0,width,height,first,rows,output,bytes,scratch,scratch_bytes,dispatch,context);
}
int cm_merge_cached_rows(const CmView frames[6],unsigned row0,unsigned span,unsigned width,unsigned height,
 unsigned first,unsigned rows,uint16_t *output,size_t bytes,void *scratch,size_t scratch_bytes){
 return merge_rows(frames,row0,span,1,width,height,first,rows,output,bytes,scratch,scratch_bytes,NULL,NULL);
}
