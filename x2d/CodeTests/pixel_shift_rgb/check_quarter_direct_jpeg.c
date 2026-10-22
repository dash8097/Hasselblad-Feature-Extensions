/* 全尺寸合成NV16，不使用用户照片。检查缩小像素、边界及HAL描述符。 */
#include "quarter_direct_jpeg.h"
#include "quarter_native_jpeg.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
static uint8_t sample(unsigned plane,unsigned x,unsigned y){return (uint8_t)(3*x+5*y+67*plane);}
int main(int argc,char **argv){
 if(argc==3){
  FILE *f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);
  assert(n>0&&n<128*1024*1024&&!fseek(f,0,SEEK_SET));
  uint8_t *p=(uint8_t*)malloc((size_t)n);assert(p&&fread(p,1,(size_t)n,f)==(size_t)n);assert(!fclose(f));
  assert(qj_quarter_visible(p,(size_t)n));f=fopen(argv[2],"wb");assert(f);
  assert(fwrite(p,1,(size_t)n,f)==(size_t)n);assert(!fclose(f));free(p);return 0;
 }
 assert(argc==1);
 const unsigned stride=11776,height=8742;
 size_t plane=(size_t)stride*height,bytes=2*plane;
 uint8_t *src=(uint8_t*)malloc(bytes),*dst=(uint8_t*)malloc(QD_BYTES+64);assert(src&&dst);
 for(unsigned p=0;p<2;p++)for(unsigned y=0;y<height;y++)for(unsigned x=0;x<stride;x++)
  src[p*plane+(size_t)y*stride+x]=sample(p,x,y);
 memset(dst,0xa5,QD_BYTES+64);QdNv16 s={src,bytes,11656,height,stride};
 assert(qd_resize(&s,dst,QD_BYTES));
 for(unsigned p=0;p<2;p++)for(unsigned y=0;y<QD_STORED_HEIGHT;y++){
  unsigned iy=y<QD_HEIGHT?y:QD_HEIGHT-1;
  for(unsigned x=0;x<QD_STRIDE;x++){
   unsigned expected=0;
   if(x<QD_WIDTH){
    unsigned sx=p?4*(x/2)+(x&1):2*x,dx=p?2:1;
    expected=((unsigned)sample(p,sx,2*iy)+sample(p,sx+dx,2*iy)+
     sample(p,sx,2*iy+1)+sample(p,sx+dx,2*iy+1)+2)/4;
   }
   assert(dst[(size_t)p*QD_STRIDE*QD_STORED_HEIGHT+(size_t)y*QD_STRIDE+x]==expected);
  }
 }
 for(unsigned i=0;i<64;i++)assert(dst[QD_BYTES+i]==0xa5);
 for(unsigned p=0;p<2;p++)for(unsigned y=0;y<height;y++)for(unsigned x=0;x<stride;x++)
  assert(src[p*plane+(size_t)y*stride+x]==sample(p,x,y));
 assert(!qd_resize(NULL,dst,QD_BYTES));assert(!qd_resize(&s,dst,QD_BYTES-1));
 assert(!qd_resize(&s,src,bytes));s.bytes--;assert(!qd_resize(&s,dst,QD_BYTES));s.bytes++;
 s.width--;assert(!qd_resize(&s,dst,QD_BYTES));s.width++;s.stride--;assert(!qd_resize(&s,dst,QD_BYTES));
 uint8_t frame[0x218],stream[0x1c8];
 assert(qd_descriptors(frame,stream,src,dst,8*1024*1024));
 assert(qj_u32(frame+0x28,0)==103&&qj_u32(frame+0x38,0)==QD_WIDTH&&qj_u32(frame+0x3c,0)==QD_STORED_HEIGHT);
 assert(qj_u32(frame+0x40,0)==QD_STRIDE&&qj_u32(frame+0x54,0)==QD_BYTES/2);
 assert(qj_u32(stream+0x30,0)==QD_WIDTH&&qj_u32(stream+0x34,0)==QD_STORED_HEIGHT);
 assert(!qd_descriptors(frame,stream,src,src,100));assert(!qd_descriptors(frame,stream,src,dst,0));
 assert(QD_QUALITY==80);free(src);free(dst);
 puts("QUARTER_DIRECT_NV16_FULL_IMAGE_BOUNDARIES_SOURCE_IMMUTABILITY_AND_DESCRIPTORS_PASS");return 0;
}
