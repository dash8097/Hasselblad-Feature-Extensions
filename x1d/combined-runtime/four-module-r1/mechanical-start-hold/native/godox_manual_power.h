#ifndef HBL_GODOX_MANUAL_POWER_H
#define HBL_GODOX_MANUAL_POWER_H

#include <stdint.h>

/* AD400Pro V1.50 参考协议：只编码业务消息，不连接设备、不发射。
 * 接收灯须已处于 M 模式；本接口不切换模式。
 * group_index: A=0 ... E=4；同组共用功率，不提供逐灯寻址。
 * attenuation_tenths: 相对全功率下降的 0.1 档数，0=1/1，80=1/256。
 * 这是协议表示范围，不保证每一种灯具支持所有档位。
 * 频道、无线 ID、前导/调制和发送时机由未来调用方处理。
 * 返回 1 才可使用输出；无效输入返回 0 并保持输出不变。
 */
static inline int hbl_godox_manual_power_encode(
        uint8_t out[4], unsigned group_index, unsigned attenuation_tenths)
{
    if (!out || group_index > 4 || attenuation_tenths > 80) return 0;
    out[0] = 0xa9;
    out[1] = (uint8_t)(0x0a + group_index);
    out[2] = 0xbc;
    out[3] = (uint8_t)attenuation_tenths;
    return 1;
}

/* 例如 1/32 +0.3 档：denominator=32、plus_tenths=3。
 * 使用整数，避免浮点舍入；不支持的分母/越过全功率均拒绝。
 */
static inline int hbl_godox_manual_power_from_fraction(
        uint8_t out[4], unsigned group_index,
        unsigned denominator, unsigned plus_tenths)
{
    unsigned stops = 0, value = denominator;
    if (!denominator || denominator > 256 ||
        (denominator & (denominator - 1)) || plus_tenths > 9) return 0;
    while (value > 1) { value >>= 1; ++stops; }
    if (plus_tenths > stops * 10) return 0;
    return hbl_godox_manual_power_encode(out, group_index, stops * 10 - plus_tenths);
}

#endif
