#ifndef X1D_DISPLAY_PIXELS_H
#define X1D_DISPLAY_PIXELS_H
#include "jpeg_container.h"
#ifdef __cplusplus
extern "C" {
#endif
/* BGRA 内存对应 ARM little-endian QImage::Format_RGB32；忽略原 X 字节并置 255。 */
XJ_EXPORT int xj_display_bgra(uint8_t *pixels, uint32_t bytes, uint32_t width, uint32_t height, int adobe);
/* 方位 1..8 原地排列。方位 5..8 交换宽高；调用者提供 ceil(width*height/8) scratch。 */
XJ_EXPORT int xj_orient_bgra(uint8_t *pixels, uint32_t bytes, uint32_t width, uint32_t height,
                            uint32_t orientation, uint8_t *scratch, uint32_t scratch_bytes);
#ifdef __cplusplus
}
#endif
#endif
