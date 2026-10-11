#ifndef HBL_BOOT_HOLD_GUARD_H
#define HBL_BOOT_HOLD_GUARD_H
#include "formal_install_hold.h"
static inline const char *bootHoldFailure(unsigned expectedPid) {
    if(holdReleased())return "hold-released";
    char data[128],extra;unsigned pid=0;unsigned long long deadline=0,pulse=0;
    if(!holdRead(holdPulsePath,data,sizeof(data)))return "hold-pulse-read";
    if(sscanf(data,"HPI1 %u %llu %llu %c",&pid,&deadline,&pulse,&extra)!=3)return "hold-pulse-format";
    if(deadline!=holdReadDeadline())return "hold-deadline-mismatch";
    if(!pid || pid!=expectedPid)return "hold-pid-mismatch";
    const uint64_t now=holdNow();
    if(!holdWindow(now,deadline))return "hold-expired";
    if(!pulse)return "hold-ui-not-ready";
    if(pulse>now)return "hold-future-pulse";
    if(now-pulse>2000)return "hold-stale-pulse";
    return nullptr;
}
#endif
