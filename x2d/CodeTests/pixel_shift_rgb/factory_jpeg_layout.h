/* 官方 4.2.0 camera-storage::toFramebuffer/toStreambuffer/Ienc::encode 布局。
 * 参数化尺寸仅代表描述符可构造；硬件尺寸能力须独立验证。 */
#ifndef FACTORY_JPEG_LAYOUT_H
#define FACTORY_JPEG_LAYOUT_H
#include <stdint.h>
#ifndef FACTORY_JPEG_MAX_SIDE
#define FACTORY_JPEG_MAX_SIDE 32768u
#endif
static void fj32(unsigned char *p,unsigned o,uint32_t v){for(unsigned i=0;i<4;i++)p[o+i]=(unsigned char)(v>>(8*i));}
static void fjptr(unsigned char *p,unsigned o,void *v){uintptr_t a=(uintptr_t)v;for(unsigned i=0;i<8;i++)p[o+i]=(unsigned char)(a>>(8*i));}
static int factory_jpeg_layout_sized(unsigned char frame[0x218],unsigned char stream[0x1c8],void *in,void *out,uint32_t capacity,uint32_t width,uint32_t height,uint32_t stride){
 uint64_t plane=(uint64_t)stride*height;
 if(!width||!height||width>FACTORY_JPEG_MAX_SIDE||height>FACTORY_JPEG_MAX_SIDE||(width&1)||(height&1)||stride<width||(stride&255)||plane*2>UINT32_MAX||!capacity)return -1;
 for(unsigned i=0;i<0x218;i++)frame[i]=0;
 for(unsigned i=0;i<0x1c8;i++)stream[i]=0;
 fjptr(frame,0x20,in);fj32(frame,0x28,103);fj32(frame,0x38,width);fj32(frame,0x3c,height);
 for(unsigned p=0;p<2;p++){unsigned o=0x40+16*p;fj32(frame,o,stride);fj32(frame,o+4,p*(uint32_t)plane);fj32(frame,o+8,height);fj32(frame,o+12,(uint32_t)plane);}
 fj32(frame,0x80,2);
 fjptr(stream,0x20,out);fj32(stream,0x28,8);fj32(stream,0x2c,103);
 fj32(stream,0x30,width);fj32(stream,0x34,height);fj32(stream,0x38,capacity);
 fj32(stream,0x48,width);fj32(stream,0x4c,height);fj32(stream,0x1b8,1);return 0;
}
static inline void factory_jpeg_layout(unsigned char frame[0x218],unsigned char stream[0x1c8],void *in,void *out,uint32_t capacity){
 (void)factory_jpeg_layout_sized(frame,stream,in,out,capacity,11656,8742,11776);
}
#endif
