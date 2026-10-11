#ifndef HBL_X1D_COMBINED_INSTALL_WINDOW_H
#define HBL_X1D_COMBINED_INSTALL_WINDOW_H
// 新组合事务独立于已经释放的引闪安装窗口；只由协调方创建/释放。
#define HBL_HOLD_STATE_DIR "/tmp/hbl-x1d-combined/install-state"
#include "../../wireless-flash/native/formal_install_hold.h"

// 同一 GUI 内供回放验证等模块只读查询；不延长窗口、不写心跳、不唤醒设备。
static inline bool hbl_combined_window_active() {
    char text[128],extra;
    unsigned pid=0;
    unsigned long long deadline=0,pulse=0;
    if(holdReleased() || !holdRead(holdPulsePath,text,sizeof(text)) ||
       sscanf(text,"HPI1 %u %llu %llu %c",&pid,&deadline,&pulse,&extra)!=3 ||
       deadline!=holdReadDeadline()) return false;
    return holdFresh(holdNow(),deadline,pulse,pid,unsigned(getpid()));
}
#endif
