/* 单张四合一的软件计算。RAW 同色四点平均，NV16 面积平均；不接触设备。 */
#ifndef QUARTER_SINGLE_CORE_H
#define QUARTER_SINGLE_CORE_H
#include <stddef.h>
#include <stdint.h>
#ifndef QUARTER_WIDTH
#define QUARTER_WIDTH 5828u
#endif
#ifndef QUARTER_HEIGHT
#define QUARTER_HEIGHT 4371u
#endif
int quarter_raw_rows(const uint16_t *,size_t,unsigned,unsigned,unsigned,unsigned,
                     unsigned,unsigned,unsigned,uint16_t *,size_t,unsigned,unsigned,unsigned);
int quarter_nv16_rows(const uint8_t *,size_t,unsigned,unsigned,unsigned,
                      uint8_t *,size_t,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
#endif
