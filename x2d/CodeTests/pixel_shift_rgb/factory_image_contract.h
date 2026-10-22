/* 官方 4.2.0，一亿像素原厂重显影输出的接收边界。
 * 只校验描述符；不导入句柄，不代表共享内存已经可访问。 */
#ifndef FACTORY_IMAGE_CONTRACT_H
#define FACTORY_IMAGE_CONTRACT_H
#include <stdint.h>
static uint32_t factory_u32(const unsigned char *p) {
    return p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
#ifndef FACTORY_IMAGE_WIDTH
#define FACTORY_IMAGE_WIDTH 11656u
#define FACTORY_IMAGE_HEIGHT 8742u
#define FACTORY_IMAGE_STRIDE 11776u
#endif
#define FACTORY_PLANE_BYTES (FACTORY_IMAGE_STRIDE * FACTORY_IMAGE_HEIGHT)
#define FACTORY_IMAGE_BYTES (2u * FACTORY_PLANE_BYTES)
_Static_assert(FACTORY_IMAGE_STRIDE >= FACTORY_IMAGE_WIDTH, "stride");
_Static_assert((uint64_t)FACTORY_IMAGE_STRIDE * FACTORY_IMAGE_HEIGHT * 2 < UINT32_MAX, "image size");
static int factory_full_yuv_header(const unsigned char *p, unsigned n) {
    if (n != 56) return 0;
    /* Full-size 8-bit YUV422sp; exact native dimensions, two planes. */
    if (factory_u32(p)!=1 || factory_u32(p+4)!=FACTORY_IMAGE_WIDTH ||
        factory_u32(p+8)!=FACTORY_IMAGE_HEIGHT || factory_u32(p+12)!=FACTORY_IMAGE_STRIDE ||
        p[16]!=6 || p[20]!=255 || factory_u32(p+52)!=2) return 0;
    /* 4.2.0 ImageMemory constructor leaves depth at 0xff for this YUV
     * format; native full response confirmed this sentinel. It is NOT 255-bit. */
    if (factory_u32(p+44)!=0 || factory_u32(p+48)!=0) return 0;
    return factory_u32(p+24)==FACTORY_IMAGE_BYTES;
}
#endif
