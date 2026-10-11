#ifndef HBL_RADIO_MODE_POLICY_H
#define HBL_RADIO_MODE_POLICY_H
#include <stdint.h>

// 无设备副作用的交接策略。只有匹配事务的完成回执才能发布新模式。
struct RadioModePolicy {
    enum Mode { Off=0, Wifi=1, Flash=2 };
    enum Phase { Stable, StopFlash, SwitchHardware, StartFlash, Failed };
    enum Action { None, DisableFlash, SetOff, SetWifi, SetFlash, EnableFlash };
    Mode current=Off, target=Off;
    Phase phase=Stable;
    uint32_t transaction=0;
    uint64_t deadline=0;
    bool verified=false;

    bool busy() const { return phase!=Stable && phase!=Failed; }
    Action request(unsigned mode, uint64_t now, bool shotActive) {
        if(mode>Flash || busy() || shotActive) return None;
        if(verified && phase==Stable && mode==unsigned(current)) return None;
        target=Mode(mode); ++transaction; if(!transaction) ++transaction;
        phase=StopFlash; deadline=now+5000; verified=false;
        return DisableFlash;
    }
    Action stopped(uint32_t ticket, uint64_t now, bool master, bool held, bool radioBusy) {
        if(ticket!=transaction || phase!=StopFlash || master || held || radioBusy) return None;
        phase=SwitchHardware; deadline=now+45000;
        return target==Wifi ? SetWifi : target==Flash ? SetFlash : SetOff;
    }
    Action hardwareDone(uint32_t ticket, uint64_t now, bool success) {
        if(ticket!=transaction || phase!=SwitchHardware) return None;
        if(!success) { phase=Failed; verified=false; return None; }
        if(target==Flash) { phase=StartFlash;deadline=now+10000;return EnableFlash; }
        current=target;phase=Stable;verified=true;return None;
    }
    void flashReady(uint32_t ticket, bool ready) {
        if(ticket!=transaction || phase!=StartFlash || !ready) return;
        current=Flash;phase=Stable;verified=true;
    }
    Action tick(uint64_t now) {
        if(!busy() || now<deadline) return None;
        phase=Failed;verified=false;return DisableFlash;
    }
};
#endif
