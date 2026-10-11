/* 离线ARM接收器候选；不包含设备入口或真实内存回调。 */
#include "boot_batch_wire.h"
int hbl_batch_receive(struct HblBatchState *s,const struct HblBatchMemory *m,const unsigned char *p,size_t n) {
    return hbl_batch_execute(s,m,p,n);
}
