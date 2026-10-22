#include "screen_policy.hpp"
namespace x2dui {
namespace {bool equal(const char16_t*a,const char16_t*b){if(!a||!b)return a==b;while(*a&&*a==*b){++a;++b;}return *a==*b;}}
void ScreenTimeoutChoices::setup(const char16_t*const*labels,std::size_t length,const char16_t*zeroName,const char16_t*currentName,bool valueIsZero){
 count=0;current=-1;valid=false;if(!labels||length>64)return;
 for(std::size_t i=0;i<length;i++){
  if(!labels[i]){count=0;return;}
  if(equal(labels[i],zeroName)||equal(labels[i],u"关闭"))continue;
  indices[count]=static_cast<int>(i);if(current<0&&equal(labels[i],currentName))current=count;++count;
 }
 if(!count)return;if(valueIsZero)current=count;valid=true;
}
ScreenChoice ScreenTimeoutChoices::select(int index)const{
 if(!valid||index<0||index>count)return {};
 return index==count?ScreenChoice{ScreenChoice::TimeoutOff,-1}:ScreenChoice{ScreenChoice::TimeoutStock,indices[index]};
}
int display_mode_index(int stockCount,int translatedIndex,bool evfOnly,bool backAllowed,bool evfAllowed){
 if(stockCount!=3)return -1;return evfOnly&&backAllowed&&evfAllowed?3:translatedIndex>=0&&translatedIndex<3?translatedIndex:-1;
}
ScreenChoice display_mode_select(int stockCount,int index){
 if(stockCount!=3||index<0||index>3)return {};return index==3?ScreenChoice{ScreenChoice::DisplaySpecial,-1}:ScreenChoice{ScreenChoice::DisplayStock,index};
}
bool ScreenModePolicy::refresh(const ScreenModeInput&s){
 enabled=s.evfOnly&&s.backAllowed&&s.evfAllowed;bool wanted=enabled&&s.evfActive;
 bool next=enabled&&s.backActive&&!s.evfActive;if(next!=intercept){half=false;notice=false;}intercept=next;
 if(!wanted){requested=false;return false;}
 if(!s.modelPresent||!s.modelEvf||requested||s.shutdown||s.upgrade||s.maintenance||s.exposing)return false;
 requested=true;return true;
}
ForwardPlan ScreenModePolicy::forward(int current,int ancestorOverride,int halfOverrideBit){
 if(!intercept)return {};owns=true;int wanted=ancestorOverride|halfOverrideBit;return {current!=wanted,wanted};
}
ForwardPlan ScreenModePolicy::release(int ancestorOverride){if(!owns)return {};owns=false;return {true,ancestorOverride};}
bool ScreenModePolicy::pressed(bool mappedHalf,bool autoRepeat,std::uint64_t now){
 if(!intercept||!mappedHalf)return false;
 if(!half&&!autoRepeat){half=true;notice=true;noticeStart=now;last=now;clockSet=true;}return true;
}
bool ScreenModePolicy::released(bool mappedHalf){if(!intercept||!mappedHalf)return false;half=false;return true;}
bool ScreenModePolicy::noticeVisible(std::uint64_t now){
 if(clockSet&&now<last)notice=false;last=now;clockSet=true;
 if(notice&&now-noticeStart>=1000)notice=false;return intercept&&notice;
}
}
