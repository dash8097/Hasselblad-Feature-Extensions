#ifndef X2D_LENS_SPEED_ALLOWLIST_H
#define X2D_LENS_SPEED_ALLOWLIST_H
#include <stdint.h>
/* X1D 八镜头身份目录为参考，不复用其速度值或地址。
 * X2D 4.2.0 lens_ctrl_get_lens_model_id 读取 lens+0x30。
 * 同版 init_lens_parameters 同时识别 90V 的 0x14f4f / 0x24f4f。
 * 未知身份、旧镜头、手动镜头拒绝增益；每次选速重新检查。 */
static int x2d_speed_lens_allowed(uint32_t key) {
    switch(key) {
        case 35u | (35u<<8) | (1u<<16): /* XCD 25V */
        case 49u | (49u<<8) | (1u<<16): /* XCD 38V */
        case 62u | (62u<<8) | (1u<<16): /* XCD 55V */
        case 79u | (79u<<8) | (1u<<16): /* XCD 90V */
        case 79u | (79u<<8) | (2u<<16): /* XCD 90V identity variant */
        case 39u | (39u<<8) | (3u<<16): /* XCD 28P */
        case 73u | (73u<<8) | (1u<<16): /* XCD 75P */
        case 28u | (47u<<8) | (1u<<16): /* XCD 20-35E */
        case 47u | (83u<<8) | (1u<<16): /* XCD 35-100E */
            return 1;
        default: return 0;
    }
}
#endif
