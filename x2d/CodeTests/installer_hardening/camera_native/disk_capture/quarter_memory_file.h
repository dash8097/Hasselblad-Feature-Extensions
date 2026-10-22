#ifndef QUARTER_MEMORY_FILE_H
#define QUARTER_MEMORY_FILE_H
#include "raw_stack_core.h"
int qm_raw_header(const uint8_t *,size_t,uint8_t *);
int qm_raw_write(const char *,const uint8_t *,const RsPixelBlock *,unsigned,const uint8_t *,size_t);
#endif
