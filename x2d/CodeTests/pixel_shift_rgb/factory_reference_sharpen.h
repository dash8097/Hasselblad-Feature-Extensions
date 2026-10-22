/* 四亿默认：强度300、半径1.2、阈值0.7；十六亿由构建参数覆盖。
 * 半径按邻域线性取样，阈值用十分之一亮度单位；只增强亮度，不宣称与 Capture One 等效。
 * 四线程原地处理亮度，线程边界原始邻域先保存，色度不变。
 * 仅必要像素运算，无逐像素校验或额外 JPEG 重压缩。
 */
#ifndef FACTORY_REFERENCE_SHARPEN_H
#define FACTORY_REFERENCE_SHARPEN_H
#include <stdint.h>
#include <stddef.h>
#ifndef FS_TEST_PTHREAD
#include <pthread.h>
#endif
#include "factory_ion_copy.h"
#define FS_MAX_WIDTH 23326u
#define FS_THREADS 4u
#ifndef FS_AMOUNT
#define FS_AMOUNT 300u
#endif
#ifndef FS_RADIUS_TENTHS
#define FS_RADIUS_TENTHS 12u
#endif
#ifndef FS_THRESHOLD_TENTHS
#define FS_THRESHOLD_TENTHS 7u
#endif
#if FS_THRESHOLD_TENTHS > 2550u
#error Unsupported sharpening threshold
#endif
#if FS_RADIUS_TENTHS == 12
/* +/-1.2 的线性取样，左右各0.25、中心0.5。 */
#define FS_KERNEL_WEIGHT {1,4,10,4,1}
#define FS_KERNEL_SUM 20
#elif FS_RADIUS_TENTHS == 15
#define FS_KERNEL_WEIGHT {1,1,4,1,1}
#define FS_KERNEL_SUM 8
#elif FS_RADIUS_TENTHS == 18
/* +/-1.8 的线性取样：左右各0.25，中心0.5。 */
#define FS_KERNEL_WEIGHT {4,1,10,1,4}
#define FS_KERNEL_SUM 20
#else
#error Unsupported sharpening radius
#endif
#define FS_BLUR_SCALE (FS_KERNEL_SUM*FS_KERNEL_SUM)
typedef struct {
 uint8_t *image,*seed,*ring;
 unsigned width,stride,first,end;
} FactorySharpenRows;
static uint8_t fs_seeds[FS_THREADS][6*FS_MAX_WIDTH];
static uint8_t fs_rings[FS_THREADS][5*FS_MAX_WIDTH];
static int fs_value(int center,int blur){
 int detail=FS_BLUR_SCALE*center-blur;
 if(detail*10>=-(int)(FS_BLUR_SCALE*FS_THRESHOLD_TENTHS)&&detail*10<=(int)(FS_BLUR_SCALE*FS_THRESHOLD_TENTHS))return center;
 int denominator=FS_BLUR_SCALE*100;
 int scaled=detail*(int)FS_AMOUNT+denominator/2;
 int delta=scaled>=0?scaled/denominator:-((-scaled+denominator-1)/denominator);
 int value=center+delta;
 return value<0?0:value>255?255:value;
}
static void fs_row(uint8_t *out,const uint8_t *const lines[5],unsigned width){
 static const unsigned weight[5]=FS_KERNEL_WEIGHT;
 unsigned x=2;
#ifdef __aarch64__
 for(;x+8<=width-2;x+=8){
  uint32x4_t lo=vdupq_n_u32(0),hi=vdupq_n_u32(0);
  for(unsigned r=0;r<5;r++){
   uint16x8_t row=vdupq_n_u16(0);
   for(unsigned c=0;c<5;c++)row=vaddq_u16(row,vmulq_n_u16(vmovl_u8(vld1_u8(lines[r]+x+c-2)),(uint16_t)weight[c]));
   lo=vaddq_u32(lo,vmulq_n_u32(vmovl_u16(vget_low_u16(row)),weight[r]));
   hi=vaddq_u32(hi,vmulq_n_u32(vmovl_u16(vget_high_u16(row)),weight[r]));
  }
  uint32_t blur[8];vst1q_u32(blur,lo);vst1q_u32(blur+4,hi);
  for(unsigned lane=0;lane<8;lane++)out[x+lane]=(uint8_t)fs_value(lines[2][x+lane],(int)blur[lane]);
 }
#endif
 for(;x+2<width;x++){
  int blur=0;
  for(unsigned r=0;r<5;r++)for(unsigned c=0;c<5;c++)blur+=(int)(weight[r]*weight[c]*lines[r][x+c-2]);
  out[x]=(uint8_t)fs_value(lines[2][x],blur);
 }
}
static void *fs_worker(void *opaque){
 FactorySharpenRows *p=opaque;unsigned w=p->width;
 for(unsigned i=0;i<4;i++)factory_ion_copy(p->ring+(size_t)i*w,p->seed+(size_t)i*w,w);
 for(unsigned r=p->first;r<p->end;r++){
  unsigned n=r-p->first;
  uint8_t *next=p->ring+(size_t)((n+4)%5)*w;
  const uint8_t *source=r+2>=p->end?p->seed+(size_t)(4+r+2-p->end)*w:p->image+(size_t)(r+2)*p->stride;
  factory_ion_copy(next,source,w);
  const uint8_t *lines[5];for(unsigned i=0;i<5;i++)lines[i]=p->ring+(size_t)((n+i)%5)*w;
  fs_row(p->image+(size_t)r*p->stride,lines,w);
 }
 return 0;
}
static int factory_reference_sharpen(uint8_t *image,unsigned width,unsigned height,unsigned stride){
 if(!image||width<5||width>FS_MAX_WIDTH||height<5||height>17498||stride<width||
    (uint64_t)height*stride>SIZE_MAX)return 0;
 FactorySharpenRows rows[FS_THREADS];pthread_t threads[FS_THREADS];unsigned started=0;
 for(unsigned i=0;i<FS_THREADS;i++){
  unsigned first=2+(height-4)*i/FS_THREADS,end=2+(height-4)*(i+1)/FS_THREADS;
  rows[i]=(FactorySharpenRows){image,fs_seeds[i],fs_rings[i],width,stride,first,end};
  for(unsigned k=0;k<4;k++)factory_ion_copy(fs_seeds[i]+(size_t)k*width,image+(size_t)(first+k-2)*stride,width);
  for(unsigned k=0;k<2;k++)factory_ion_copy(fs_seeds[i]+(size_t)(4+k)*width,image+(size_t)(end+k)*stride,width);
 }
 for(;started<FS_THREADS;started++)if(pthread_create(threads+started,0,fs_worker,rows+started))break;
 for(unsigned i=0;i<started;i++)if(pthread_join(threads[i],0))factory_sharpen_join_failure();
 return started==FS_THREADS;
}
#endif
