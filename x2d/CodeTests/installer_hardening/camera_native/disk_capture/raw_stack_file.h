#ifndef RAW_STACK_FILE_H
#define RAW_STACK_FILE_H
#include <stddef.h>
#include <stdint.h>
#include "raw_stack_core.h"
int rs_raw_header(const uint8_t source[16384],size_t preview_bytes,uint8_t out[16384]);
/* 仅创建新成片。失败只清理本调用独占创建的文件，不覆盖既有照片。 */
int rs_raw_write(const char *,const uint8_t header[16384],const uint16_t *,size_t,
 const uint8_t *preview,size_t preview_bytes);
int rs_raw_write_blocks(const char *,const uint8_t[16384],const RsPixelBlock *,unsigned,
 const uint8_t *,size_t);
#endif
