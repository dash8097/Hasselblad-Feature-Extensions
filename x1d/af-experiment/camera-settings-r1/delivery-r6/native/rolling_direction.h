#ifndef ROLLING_DIRECTION_H
#define ROLLING_DIRECTION_H
#include "native_af.h"
/* 只计算原厂 accepted 空间趋势，不声明物理帧/采样时刻已关联。 */
float as_direction(const s32 *position,const u32 *cv,u32 count,u32 cycle_minimum);
#endif
