/* 只约束同次 AF-S 的重新提快档；不产生镜头位置、方向或停止命令。 */
#ifndef X2D_NEAR_GUARD_H
#define X2D_NEAR_GUARD_H
#include <stdint.h>
typedef struct { unsigned slow, far_count, far_frame, have_far_frame; } NearGuard;
static void near_clear(NearGuard *s) { *s=(NearGuard){0}; }
/* 原厂真正执行距离比较且结果为远，才累计解除依据。重复处理同帧不累计。 */
static int near_distance(NearGuard *s,int value,int previous,int compared,uint32_t frame) {
    if(value==1||value==2) {
        s->slow=(unsigned)value;s->far_count=0;s->have_far_frame=0;return value;
    }
    if(value!=0) { near_clear(s);return value; }
    if(compared) {
        if(!s->have_far_frame||s->far_frame!=frame) {
            s->far_frame=frame;s->have_far_frame=1;
            if(s->far_count<2)++s->far_count;
        }
        if(s->far_count>=2) { s->slow=0;return 0; }
    } else { s->far_count=0;s->have_far_frame=0; }
    return s->slow&&(previous==1||previous==2)?previous:0;
}
static unsigned near_speed(NearGuard *s,unsigned proposed) {
    if(proposed==0&&s->slow)return s->slow;
    /* 档1和档2之间仍允许原厂调整；固定的相位慢扫不在此入口，不作为近焦证据。 */
    if(proposed==1||proposed==2)s->slow=proposed;
    return proposed;
}
#endif
