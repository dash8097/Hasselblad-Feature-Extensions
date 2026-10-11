/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 四亿模式输出及回放边界。只接受上层已校验的本项目成片关联，不按扩展名猜身份。 */
#ifndef FOUR_HUNDRED_POLICY_H
#define FOUR_HUNDRED_POLICY_H
#include <stdint.h>

enum four_hundred_route { FOUR_HUNDRED_ORIGINAL=0, FOUR_HUNDRED_JPEG=1, FOUR_HUNDRED_UNAVAILABLE=2 };
struct four_hundred_output {
    unsigned write_jpeg;
    unsigned write_raw;
};
struct four_hundred_pair {
    unsigned owned_capture;       /* 已校验本项目合成事务标记及身份 */
    unsigned committed;           /* 完整输出发布标记，不能仅凭文件存在 */
    unsigned jpeg_validated;      /* 内容完整性、尺寸、预览及对应关系已验证 */
    unsigned same_capture;        /* JPEG 与照片项属于同一合成事务 */
    uint32_t width,height;
};
static inline int four_hundred_output_policy(unsigned raw_enabled,unsigned jpeg_enabled,
                                             struct four_hundred_output *out) {
    if(!out || raw_enabled>1 || jpeg_enabled>1)return 0;
    out->write_jpeg=1;
    out->write_raw=raw_enabled;
    return 1;
}
static inline enum four_hundred_route four_hundred_playback_policy(const struct four_hundred_pair *pair) {
    if(!pair || !pair->owned_capture)return FOUR_HUNDRED_ORIGINAL;
    if(pair->owned_capture!=1 || pair->committed!=1 || pair->jpeg_validated!=1 ||
       pair->same_capture!=1 || pair->width!=23326 || pair->height!=17498)
        return FOUR_HUNDRED_UNAVAILABLE;
    return FOUR_HUNDRED_JPEG;
}
#endif
