/* 原厂计数器只在画面变化通过检查时增加；不修改计数器或原厂参数。 */
#ifndef X2D_AFC_SETTLE_POLICY_H
#define X2D_AFC_SETTLE_POLICY_H
static int afc_settle_result(int original, unsigned channel, int afc,
                             unsigned stable, unsigned fps100) {
    if (original != 1 || channel != 0 || afc != 1 ||
        fps100 == 0 || fps100 > 24000) return original;
    /* 1/30 秒；fps100 为原厂帧率乘 100，四舍五入且至少一帧。 */
#ifdef X2D_AFC_SETTLE_ONE_FRAME
    unsigned frames = 1;
#else
    unsigned frames = (fps100 + 1500) / 3000;
#endif
    if (!frames) frames = 1;
    return stable >= frames ? 0 : original;
}
#endif
