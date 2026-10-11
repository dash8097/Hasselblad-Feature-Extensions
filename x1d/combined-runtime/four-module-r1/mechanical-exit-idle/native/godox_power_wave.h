#ifndef HBL_GODOX_POWER_WAVE_H
#define HBL_GODOX_POWER_WAVE_H

#include "godox_manual_power.h"
#include <stddef.h>

/* 固定 CH5 / ID5 的功率包候选。采样参数来自哈希绑定的已验证普通包，
 * 仅复用其前导、同步及调制；本头文件没有设备访问或发射入口。
 */
enum { HBL_POWER_FRAME_BYTES=12, HBL_POWER_RUNS=96,
       HBL_POWER_LEADING_ZERO_SAMPLES=256, HBL_POWER_TRAILING_ZERO_SAMPLES=0x31fc,
       HBL_POWER_TOTAL_SAMPLES=0x6f00 };
typedef struct {
    uint16_t samples, reserved;
    uint32_t phase_step;
} HblGodoxPowerRun;

static inline int hbl_godox_power_frame_encode(uint8_t out[HBL_POWER_FRAME_BYTES],
                                               unsigned group, unsigned attenuation)
{
    uint8_t command[4];
    if (!out || !hbl_godox_manual_power_encode(command,group,attenuation)) return 0;
    for (unsigned i=0;i<4;++i) out[i]=0xaa;
    out[4]=0x20; out[5]=0x91; out[6]=0x20; out[7]=0x91;
    for (unsigned i=0;i<4;++i) out[8+i]=command[i];
    return 1;
}

static inline int hbl_godox_power_runs_encode(HblGodoxPowerRun out[HBL_POWER_RUNS],
                                              unsigned group, unsigned attenuation)
{
    uint8_t frame[HBL_POWER_FRAME_BYTES];
    if (!out || !hbl_godox_power_frame_encode(frame,group,attenuation)) return 0;
    for (unsigned i=0;i<HBL_POWER_RUNS;++i) {
        const unsigned bit=(frame[i/8]>>(7-i%8))&1u;
        out[i].samples=(uint16_t)(160+(i==0 || i==25 || i==51 || i==76));
        out[i].reserved=0;
        out[i].phase_step=bit ? 261686886u : 275105382u;
    }
    return 1;
}

/* 与固定波形读回函数相同的逐 uint32_t FNV；这是模拟样本的校验值，
 * 不是射频测量。无效输入不写 out_hash。
 */
static inline int hbl_godox_power_wave_hash(uint32_t *out_hash,
        const HblGodoxPowerRun runs[HBL_POWER_RUNS], const uint32_t lut[1024])
{
    uint32_t hash=0x811c9dc5u,phase=0;
    if (!out_hash || !runs || !lut) return 0;
    for (unsigned i=0;i<HBL_POWER_RUNS;++i) {
        const unsigned expected=160+(i==0 || i==25 || i==51 || i==76);
        if (runs[i].samples!=expected || runs[i].reserved ||
            (runs[i].phase_step!=261686886u && runs[i].phase_step!=275105382u)) return 0;
    }
    for (unsigned i=0;i<HBL_POWER_LEADING_ZERO_SAMPLES;++i) hash*=0x01000193u;
    for (unsigned i=0;i<HBL_POWER_RUNS;++i) {
        for (unsigned j=0;j<runs[i].samples;++j) {
            phase+=runs[i].phase_step;
            hash=(hash^lut[phase>>22])*0x01000193u;
        }
    }
    for (unsigned i=0;i<HBL_POWER_TRAILING_ZERO_SAMPLES;++i) hash*=0x01000193u;
    *out_hash=hash;
    return 1;
}

#endif
