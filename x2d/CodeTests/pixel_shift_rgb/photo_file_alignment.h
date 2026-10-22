/* 成片只在编码流之后补零；TIFF/FlashPix 中的图像长度保持真实值。 */
#ifndef PHOTO_FILE_ALIGNMENT_H
#define PHOTO_FILE_ALIGNMENT_H
#include <stddef.h>
#include <stdint.h>
static inline size_t photo_eof_padding(uint64_t bytes){
 return (size_t)((4096u-(bytes&4095u))&4095u);
}
#endif
