#pragma once
#include <cstdint>
#include <cstddef>
namespace x2dui {
// 独立取景倒计时：只有完整就绪快照才能启动；无活动不重复退出。
class LiveviewTimeoutPolicy {
 std::uint64_t start_=0,last_=0;int seconds_=0;
 bool clock_=false,armed_=false,expired_=false;
public:
 static bool valid_seconds(int n){return n==0||n==10||n==20||n==30||n==60||n==120||n==300;}
 bool update(std::uint64_t now,int seconds,bool eligible,bool touching,bool activity){
  bool changed=seconds!=seconds_;bool backwards=clock_&&now<last_;last_=now;clock_=true;seconds_=seconds;
  if(!valid_seconds(seconds)||!seconds||!eligible||touching){armed_=expired_=false;start_=now;return false;}
  if(changed||backwards||activity){armed_=expired_=false;start_=now;}
  if(expired_)return false;
  if(!armed_){armed_=true;start_=now;return false;}
  if(now-start_<std::uint64_t(seconds)*1000)return false;
  armed_=false;expired_=true;return true;
 }
 bool waiting()const{return armed_;}
};
struct ScreenChoice {enum Kind {None,DisplaySpecial,DisplayStock,TimeoutOff,TimeoutStock};Kind kind=None;int sourceIndex=-1;};
struct ScreenTimeoutChoices {
 int indices[64]{},count=0,current=-1;bool valid=false;
 void setup(const char16_t*const*labels,std::size_t length,const char16_t*zeroName,const char16_t*currentName,bool valueIsZero);
 ScreenChoice select(int index)const;
};
int display_mode_index(int stockCount,int translatedIndex,bool evfOnly,bool backAllowed,bool evfAllowed);
ScreenChoice display_mode_select(int stockCount,int index);
struct ScreenModeInput {
 bool evfOnly=false,backAllowed=false,evfAllowed=false,backActive=false,evfActive=false;
 bool modelPresent=false,modelEvf=false,shutdown=false,upgrade=false,maintenance=false,exposing=false;
};
struct ForwardPlan {bool write=false;int value=0;};
class ScreenModePolicy {
 bool enabled=false,intercept=false,requested=false,owns=false,half=false,notice=false;
 std::uint64_t noticeStart=0,last=0;bool clockSet=false;
public:
 // 返回true表示一次原厂setLiveView请求；不产生任何AF/快门指令。
 bool refresh(const ScreenModeInput&);
 bool intercepting()const{return intercept;}
 bool modeEnabled()const{return enabled;}
 ForwardPlan forward(int current,int ancestorOverride,int halfOverrideBit);
 ForwardPlan release(int ancestorOverride);
 bool pressed(bool mappedHalf,bool autoRepeat,std::uint64_t now);
 bool released(bool mappedHalf);
 bool noticeVisible(std::uint64_t now);
 void close(){intercept=false;half=false;notice=false;requested=false;}
};
}
