#ifndef QUARTER_SOFTWARE_JPEG_H
#define QUARTER_SOFTWARE_JPEG_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 调用者拥有返回内存并free；只接受固定4.2.0一亿基线JPEG，行流式半尺寸解码。 */
int qn_jpeg_half(const uint8_t*,size_t,uint8_t**,size_t*);
#ifdef __cplusplus
}
#endif
#endif
