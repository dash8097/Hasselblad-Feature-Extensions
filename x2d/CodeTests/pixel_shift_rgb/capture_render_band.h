/* 九批横向行带的内存输入适配；描述符算术不等于原厂接受这些尺寸。
 * 保留已有 RAW 填充模板的上 96 行、下 4 行周边和六合一横向相位。
 * 原厂滤波的完整空间标定适用性仍待实机验证。 */
#ifndef CAPTURE_RENDER_BAND_H
#define CAPTURE_RENDER_BAND_H
#include "capture_memory_pipeline.h"
#include "factory_raw_tile.h"
#include "factory_buffer_geometry.h"
#define CM_BAND_WIDTH 23326u
#define CM_BAND_HEIGHT 17498u
typedef struct {
 unsigned first_row,rows;
 FactoryBufferGeometry raw,yuv;
} CmRenderBand;
typedef struct {const CmpBuffer *buffer;unsigned width;} CmBandRead;
static inline int cm_band_plan(unsigned first_row,unsigned rows,CmRenderBand *out){
 if(!out||!rows||rows>2048||(first_row&1)||(rows&1)||
    first_row>=CM_BAND_HEIGHT||rows>CM_BAND_HEIGHT-first_row)return 0;
 CmRenderBand plan={.first_row=first_row,.rows=rows};
 if(!factory_buffer_geometry(&plan.yuv,CM_BAND_WIDTH+2,rows,6)||
    !factory_buffer_geometry(&plan.raw,CM_BAND_WIDTH+182,rows+100,1))return 0;
 *out=plan;return 1;
}
static int cm_band_read(void *context,uint64_t offset,unsigned char *out,uint32_t bytes){
 CmBandRead *r=context;const CmpBuffer *b=r->buffer;
 uint64_t start=(uint64_t)b->first_row*r->width*2;
 uint64_t length=(uint64_t)b->rows*r->width*2;
 if(!b->busy||!out||offset<start||offset-start>length||bytes>length-(offset-start))return 0;
 memcpy(out,(const uint8_t*)b->pixels+(size_t)(offset-start),bytes);return 1;
}
static inline int cm_band_fill(const PcChunk *chunk,const CmRenderBand *plan,
 uint16_t *target,size_t capacity,uint16_t *cache0,uint16_t *cache1){
 if(!chunk||!plan||!chunk->owner||!target||!cache0||!cache1||
    chunk->first_row!=plan->first_row||chunk->rows!=plan->rows||
    capacity<plan->raw.allocation_bytes)return 0;
 CmpBuffer *b=chunk->owner;CmBandRead read={b,CM_BAND_WIDTH};
 CmRenderBand expected;
 if(!cm_band_plan(plan->first_row,plan->rows,&expected)||
    memcmp(plan,&expected,sizeof expected)||!b->pixels||
    ((uintptr_t)target&1)||((uintptr_t)b->pixels&1)||
    b->first_row>=CM_BAND_HEIGHT||b->rows>CM_BAND_HEIGHT-b->first_row||
    (uint64_t)b->rows*CM_BAND_WIDTH*2>b->capacity)return 0;
 uint64_t valid_offset=(uint64_t)(chunk->first_row-b->first_row)*CM_BAND_WIDTH*2;
 if(!b->busy||chunk->first_row<b->first_row||valid_offset>b->capacity||
    chunk->bytes>b->capacity-valid_offset||
    chunk->pixels!=(const uint8_t*)b->pixels+valid_offset)return 0;
 if(!factory_raw_tile(cm_band_read,&read,target,CM_BAND_WIDTH,CM_BAND_HEIGHT,CM_BAND_WIDTH,
    plan->raw.width,plan->raw.height,plan->raw.stride_pixels,
    0,plan->first_row,129,96,cache0,cache1))return 0;
 memset((uint8_t*)target+plan->raw.content_bytes,0,
        plan->raw.allocation_bytes-plan->raw.content_bytes);
 return 1;
}
#endif
