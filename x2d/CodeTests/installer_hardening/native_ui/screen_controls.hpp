#pragma once
#include "screen_policy.hpp"
namespace x2dui {
enum class ScreenControlKind {Display,Timeout,AdaptiveFps,LiveviewTimeout};
enum class ScreenWriteKind {EvfOnly,ScreensAllowed,DisplayStock,TimeoutZero,TimeoutStock,AdaptiveFps,LiveviewTimeout};
struct ScreenWrite {ScreenWriteKind kind;int value=0,sourceIndex=-1;const char16_t*label=nullptr;};
struct ScreenControlApi {void*context=nullptr;bool(*apply)(void*,const ScreenWrite&)=nullptr;};
struct ScreenControlSnapshot {
 bool enabled=false,evfOnly=false,timeoutZero=false,fps=false,liveviewConfirmed=false;int screensAllowed=0,backBit=0,evfBit=0,liveviewSeconds=0;
 const char16_t*const*labels=nullptr;size_t count=0;const char16_t*current=nullptr;const char16_t*zero=nullptr;
};
class ScreenControl {
 ScreenControlKind kind_;ScreenControlApi api_;ScreenControlSnapshot state_;
 char16_t labels_[65][128]{};int source_[65]{};int count_=0,current_=-1,wanted_=-1,sourceWanted_=-1;
 char16_t target_[128]{},currentName_[128]{};int requestedBack_=0,requestedEvf_=0;bool valid_=false,busy_=false,error_=false,complete_=false,dispatching_=false;int phase_=0;uint64_t deadline_=0;
 bool send(ScreenWrite);bool target_equal(const char16_t*)const;void fail();void second();
public:
 ScreenControl(ScreenControlKind k,ScreenControlApi a):kind_(k),api_(a){}
 bool update(const ScreenControlSnapshot&);
 bool choose(int index,uint64_t now);void tick(uint64_t now);void close();
 ScreenControlKind kind()const noexcept{return kind_;}int count()const noexcept{return count_;}int current()const noexcept{return current_;}
 const char16_t*label(int i)const noexcept{return i>=0&&i<count_?labels_[i]:u"";}
 bool available()const noexcept{return valid_&&state_.enabled&&!busy_;}bool busy()const noexcept{return busy_;}bool error()const noexcept{return error_;}
 bool dispatching()const noexcept{return dispatching_;} // owner在false之后才可析构。
 bool take_complete()noexcept{bool c=complete_;complete_=false;return c;}
 const char16_t*title()const noexcept;const char16_t*description()const noexcept;
};
}
