#include "quarter_direct_jpeg.h"
#include "quarter_single_core.h"
#include "factory_jpeg_layout.h"
#include <string.h>
int qd_resize(const QdNv16 *s,uint8_t *out,size_t capacity){
 if(!s||s->width!=11656||s->height!=8742||s->stride<s->width||
    (s->stride&255)||capacity<QD_BYTES)return 0;
 if(!quarter_nv16_rows(s->pixels,s->bytes,s->width,s->height,s->stride,
    out,capacity,QD_WIDTH,QD_HEIGHT,QD_STRIDE,QD_STORED_HEIGHT,0,QD_HEIGHT))return 0;
 for(unsigned plane=0;plane<2;plane++){
  uint8_t *last=out+(size_t)plane*QD_STRIDE*QD_STORED_HEIGHT+(size_t)QD_HEIGHT*QD_STRIDE;
  memcpy(last,last-QD_STRIDE,QD_STRIDE);
 }
 return 1;
}
int qd_descriptors(uint8_t frame[0x218],uint8_t stream[0x1c8],
 void *pixels_handle,void *jpeg_handle,uint32_t jpeg_capacity){
 if(!frame||!stream||!pixels_handle||!jpeg_handle||pixels_handle==jpeg_handle)return 0;
 return factory_jpeg_layout_sized(frame,stream,pixels_handle,jpeg_handle,jpeg_capacity,
  QD_WIDTH,QD_STORED_HEIGHT,QD_STRIDE)==0;
}
