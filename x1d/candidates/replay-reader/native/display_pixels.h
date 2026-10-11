#ifndef X1D_DISPLAY_PIXELS_H
#define X1D_DISPLAY_PIXELS_H
#include "jpeg_container.h"
#ifdef __cplusplus
extern "C" {
#endif
/* BGRA 内存对应 ARM little-endian QImage::Format_RGB32；忽略原 X 字节并置 255。 */
XJ_EXPORT int xj_display_bgra(uint8_t *pixels, uint32_t bytes, uint32_t width, uint32_t height, int adobe);
/* 合法尺寸下所需 scratch；方位 1..4 为零，其他不超过旧 ceil(width*height/8)。 */
XJ_EXPORT uint32_t xj_orient_scratch_bytes(uint32_t width, uint32_t height, uint32_t orientation);
/* 方位 1..8 原地排列。方位 5..8 交换宽高；不分配第二张整图。
 * 所需 scratch 由上式取得；另有固定最多 2048 字节的栈上块缓冲。
 * 参数、容量和重叠检查失败时不改变输入像素。 */
XJ_EXPORT int xj_orient_bgra(uint8_t *pixels, uint32_t bytes, uint32_t width, uint32_t height,
                            uint32_t orientation, uint8_t *scratch, uint32_t scratch_bytes);
#ifdef __cplusplus
}
#endif
#endif
