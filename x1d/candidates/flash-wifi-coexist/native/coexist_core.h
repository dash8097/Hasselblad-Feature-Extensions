#ifndef HBL_COEXIST_CORE_H
#define HBL_COEXIST_CORE_H
#include <stdint.h>

namespace coexist {
// 固定首轮参数。功率刻度沿用原正式协议：0=1/256，50=1/8。
enum { Channel=5, Id=5, GroupD=3, PowerTenths=50 };
enum class Phase { Disabled, Network, SaveNetwork, OpenFlash, Power,
                   Exposing, CloseFlash, RestoreNetwork, Fault };
enum class Action { None, SaveNetwork, OpenFlash, SendPower, ContinueExposure,
                    CancelShot, CloseFlash, RestoreNetwork };
enum class Error { None, Busy, LinkUnavailable, Radio, Cancelled, TimedOut, Unknown };
struct Command {
    Action action;
    uint32_t token;
    uint64_t serial;
    Command(Action a=Action::None,uint32_t t=0,uint64_t s=0):action(a),token(t),serial(s) {}
};

// 纯业务核心不执行任何 I/O。一个线程拥有，适配层把真正的完成/失败回传。
// 不把启动子进程、发送本地消息或一份陈旧状态当成完成。
class Core {
public:
    Phase phase=Phase::Disabled;
    Error error=Error::None;
    bool enabled=false,linkReady=false,radioReleased=true,exposureAllowed=false;
    uint32_t token=0,lastToken=0;
    uint64_t deadline=0;
    explicit Core(uint64_t operationTimeoutMs=10000):timeoutMs(operationTimeoutMs) {}
    bool enable(bool on) {
        if(on) {
            if(phase!=Phase::Disabled && phase!=Phase::Network) return false;
            enabled=true;phase=Phase::Network;error=Error::None;return true;
        }
        enabled=false;
        if(phase==Phase::Disabled) return true;
        if(phase==Phase::Network) {phase=Phase::Disabled;return true;}
        return cancel();
    }
    void networkStatus(bool ready) { linkReady=ready; }
    bool press(uint32_t next,uint64_t now) {
        if(!enabled || phase!=Phase::Network || pending.action!=Action::None) {error=Error::Busy;return false;}
        if(!linkReady) {error=Error::LinkUnavailable;return false;}
        if(!next || next<=lastToken) return false;
        token=lastToken=next;error=Error::None;exposureAllowed=false;cancelled=false;
        phase=Phase::SaveNetwork;issue(Action::SaveNetwork,now);return true;
    }
    Command take() {
        if(delivered) return Command();
        delivered=pending.action!=Action::None;return pending;
    }
    bool complete(Command command,bool ok,uint64_t now) {
        if(!delivered || command.action!=pending.action || command.token!=pending.token ||
           command.serial!=pending.serial || command.action==Action::None) return false;
        pending=Command();delivered=false;deadline=0;
        switch(command.action) {
        case Action::SaveNetwork:
            if(!ok) { linkReady=false;finish(Error::LinkUnavailable);break; }
            if(cancelled) {phase=Phase::RestoreNetwork;issue(Action::RestoreNetwork,now);break;}
            radioReleased=false;phase=Phase::OpenFlash;issue(Action::OpenFlash,now);break;
        case Action::OpenFlash:
            if(!ok || cancelled) {error=ok ? Error::Cancelled : Error::Radio;close(now);break;}
            phase=Phase::Power;issue(Action::SendPower,now);break;
        case Action::SendPower:
            if(!ok || cancelled) {error=ok ? Error::Cancelled : Error::Radio;close(now);break;}
            phase=Phase::Exposing;issue(Action::ContinueExposure,now);break;
        case Action::ContinueExposure:
            if(!ok || cancelled) {error=ok ? Error::Cancelled : Error::Radio;issue(Action::CancelShot,now);break;}
            exposureAllowed=true;break;
        case Action::CancelShot:
            if(!ok) {phase=Phase::Fault;error=Error::Unknown;break;}
            close(now);break;
        case Action::CloseFlash:
            if(!ok) {phase=Phase::Fault;error=Error::Unknown;radioReleased=false;break;}
            radioReleased=true;phase=Phase::RestoreNetwork;issue(Action::RestoreNetwork,now);break;
        case Action::RestoreNetwork:
            if(!ok) {phase=Phase::Fault;error=Error::LinkUnavailable;linkReady=false;break;}
            linkReady=true;finish(error);break;
        default:return false;
        }
        return true;
    }
    bool shotEnded(uint32_t ended,uint64_t now) {
        if(!ended || ended!=token || phase!=Phase::Exposing || pending.action!=Action::None || !exposureAllowed) return false;
        exposureAllowed=false;close(now);return true;
    }
    bool cancel(uint64_t now=0) {
        if(phase==Phase::Disabled || phase==Phase::Network || phase==Phase::Fault) return false;
        // 原厂曝光已经开始后，仅禁止下一张；等待真实结束事件后才交还射频。
        if(phase==Phase::Exposing && exposureAllowed && pending.action==Action::None) {
            cancelled=true;error=Error::Cancelled;return true;
        }
        cancelled=true;exposureAllowed=false;error=Error::Cancelled;
        // 已提交操作只等待其明确完成；不得同时切频或把旧完成绑定到下一张。
        if(pending.action!=Action::None) return true;
        if(phase==Phase::Exposing) issue(Action::CancelShot,now);
        else if(!radioReleased) close(now);
        return true;
    }
    bool tick(uint64_t now) {
        if(!deadline || now<deadline || pending.action==Action::None) return false;
        // 超时意味着完成未知，不在同一硬件上并发执行恢复。
        phase=Phase::Fault;error=Error::TimedOut;exposureAllowed=false;cancelled=true;return true;
    }
    bool pendingUnknown() const { return phase==Phase::Fault && pending.action!=Action::None; }
private:
    uint64_t timeoutMs,sequence=0;
    Command pending;
    bool delivered=false,cancelled=false;
    void issue(Action action,uint64_t now) {
        pending=Command(action,token,++sequence);delivered=false;deadline=now+timeoutMs;
    }
    void close(uint64_t now) {phase=Phase::CloseFlash;issue(Action::CloseFlash,now);}
    void finish(Error result) {
        phase=enabled ? Phase::Network : Phase::Disabled;error=result;
        token=0;exposureAllowed=false;cancelled=false;deadline=0;
    }
};
}
#endif
