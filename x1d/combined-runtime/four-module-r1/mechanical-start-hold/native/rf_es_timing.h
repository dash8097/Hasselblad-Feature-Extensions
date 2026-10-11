#ifndef HBL_RF_ES_TIMING_H
#define HBL_RF_ES_TIMING_H
#include <stdint.h>
#include "rf_core.h"

/* 用户实拍指定的近似电子快门模型；全部输入为实际曝光微秒数。
 * 不读硬件、不改变原厂补偿。返回 0 表示该曝光不自动引闪。
 */
static inline int rf_es_center_delay(uint32_t exposure_low,uint32_t exposure_high,
                                     unsigned *delay_ms) {
    if (!delay_ms || exposure_high || !exposure_low || exposure_low>500000u) return 0;
    uint32_t end_us;
    if (exposure_low<=250000u) end_us=590000u;
    else if (exposure_low>=295000u) end_us=890000u;
    else return 0; /* 未覆盖的档位，不猜测分组或全开窗口。 */
    /* E - (扫描 295 ms + 曝光 T)/2，取整到最近毫秒。 */
    *delay_ms=(2u*end_us-295000u-exposure_low+1000u)/2000u;
    return 1;
}
/* 保持界面配置为自动；只给当前已消费的 B 事件设置独立截止时间。 */
static inline int rf_es_set_deadline(RfCore *core,unsigned delay_ms,uint64_t arrival_ms) {
    if (!core || !core->enabled || !core->active || !core->consumed || !core->pending ||
        core->source || core->phase_seen!=1 || delay_ms>1000 ||
        arrival_ms!=core->last_ms || arrival_ms>UINT64_MAX-delay_ms) return 0;
    core->deadline_ms=arrival_ms+delay_ms;
    return 1;
}
#endif
