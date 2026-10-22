#pragma once
#include "screen_policy.hpp"
namespace x2dui {
struct ScreenModeApi {
 void*context=nullptr;
 bool(*liveview)(void*)=nullptr; // 固定原厂stateModel.setLiveView(true)，不发半按/AF。
 bool(*forward)(void*,int)=nullptr;
 bool(*keys)(void*,void*owner,bool attach)=nullptr; // 仅追加/移除自身，保留原列表。
};
struct ScreenModeSnapshot {ScreenModeInput mode;void*focusOwner=nullptr;int forward=0,ancestorOverride=0,halfOverrideBit=0;};
class ScreenModeSession {
 ScreenModeApi api_;ScreenModePolicy policy_;ScreenModeSnapshot s_;void*owner_=nullptr;
 bool installed_=false,attempted_=false,pending_=false,haveDesired_=false,confirmed_=false,closed_=false,dispatching_=false,updating_=false,cleanupPending_=false;int desired_=0;unsigned failures_=0;
 void write(int);void releaseOwner();
public:
 explicit ScreenModeSession(ScreenModeApi a):api_(a){}
 void update(const ScreenModeSnapshot&);
 bool press(bool mappedHalf,bool repeat,uint64_t now);
 bool release(bool mappedHalf);
 bool notice(uint64_t now){return confirmed_&&installed_&&policy_.noticeVisible(now);}
 void close();
 void close_with_override(int value){if(value>=0&&value<=255)s_.ancestorOverride=value;close();}
 void invalidate_keys(){if(!dispatching_&&!updating_){releaseOwner();confirmed_=false;}}
 bool intercepting()const noexcept{return confirmed_&&installed_&&policy_.intercepting();}
 bool pending()const noexcept{return pending_;}unsigned failures()const noexcept{return failures_;}
 bool dispatching()const noexcept{return dispatching_||updating_;} // 同步回调中不得销毁owner；close可立即标记并延迟清理。
};
}
