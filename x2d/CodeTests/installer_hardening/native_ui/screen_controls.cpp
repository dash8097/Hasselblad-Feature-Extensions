#include "screen_controls.hpp"
namespace x2dui {namespace {
bool copy(char16_t*to,const char16_t*from){if(!from)return false;size_t n=0;while(n<128&&from[n])++n;if(n==128)return false;for(size_t i=0;i<=n;i++)to[i]=from[i];return true;}
bool eq(const char16_t*a,const char16_t*b){if(!a||!b)return false;size_t n=0;for(;n<128&&a[n]&&a[n]==b[n];n++){}return n<128&&a[n]==b[n];}
bool bit(int v){return v>0&&(v&(v-1))==0;}
}
const char16_t*ScreenControl::title()const noexcept{return kind_==ScreenControlKind::Display?u"显示模式":kind_==ScreenControlKind::Timeout?u"息屏时间":kind_==ScreenControlKind::LiveviewTimeout?u"取景超时":u"自适应取景帧率";}
const char16_t*ScreenControl::description()const noexcept{return kind_==ScreenControlKind::AdaptiveFps?u"开启后可降低耗电、提升弱光取景画质，但会降低对焦性能，包括弱光下的对焦性能。":u"";}
bool ScreenControl::update(const ScreenControlSnapshot&s){if(dispatching_)return false;valid_=false;count_=0;current_=-1;state_=s;
 if(kind_==ScreenControlKind::AdaptiveFps){copy(labels_[0],u"关闭");copy(labels_[1],u"开启");source_[0]=0;source_[1]=1;count_=2;current_=s.fps?1:0;valid_=true;return true;}
 if(kind_==ScreenControlKind::LiveviewTimeout){
  static const int values[]={0,10,20,30,60,120,300};
  if(!s.labels||s.count!=7||!LiveviewTimeoutPolicy::valid_seconds(s.liveviewSeconds))return false;
  for(int i=0;i<7;i++){if(!copy(labels_[i],s.labels[i]))return false;source_[i]=values[i];if(s.liveviewSeconds==values[i])current_=i;}
  count_=7;valid_=true;return true;
 }
 if(!s.labels||!s.current||s.count>64||!s.count||!copy(currentName_,s.current))return false;state_.current=currentName_;state_.labels=nullptr;state_.zero=nullptr;char16_t checked[128];for(size_t i=0;i<s.count;i++)if(!copy(checked,s.labels[i]))return false;if(s.zero&&!copy(checked,s.zero))return false;
 if(kind_==ScreenControlKind::Display){if(s.count!=3||!bit(s.backBit)||!bit(s.evfBit)||s.backBit==s.evfBit)return false;for(size_t i=0;i<s.count;i++){if(!copy(labels_[i],s.labels[i]))return false;source_[i]=int(i);if(eq(s.labels[i],s.current))current_=int(i);}count_=3;copy(labels_[count_],u"屏幕调参");source_[count_]=-1;++count_;current_=display_mode_index(3,current_,s.evfOnly,(s.screensAllowed&s.backBit)!=0,(s.screensAllowed&s.evfBit)!=0);}
 else{ScreenTimeoutChoices c;c.setup(s.labels,s.count,s.zero,s.current,s.timeoutZero);if(!c.valid)return false;for(int i=0;i<c.count;i++){source_[i]=c.indices[i];if(!copy(labels_[i],s.labels[c.indices[i]]))return false;}count_=c.count;copy(labels_[count_],u"关闭");source_[count_]=-1;++count_;current_=c.current;}
 valid_=true;return true;
}
bool ScreenControl::send(ScreenWrite w){if(dispatching_)return false;dispatching_=true;bool ok=api_.apply&&api_.apply(api_.context,w);dispatching_=false;if(!ok){fail();return false;}return true;}
void ScreenControl::fail(){busy_=false;error_=true;phase_=0;}
bool ScreenControl::target_equal(const char16_t*s)const{return eq(target_,s);}
bool ScreenControl::choose(int index,uint64_t now){if(dispatching_||!available()||index<0||index>=count_||now>UINT64_MAX-5000)return false;wanted_=index;requestedBack_=state_.backBit;requestedEvf_=state_.evfBit;sourceWanted_=source_[index];if(!copy(target_,labels_[index]))return false;busy_=true;error_=complete_=false;deadline_=now+5000;
 if(kind_==ScreenControlKind::Display){phase_=1;return send({ScreenWriteKind::EvfOnly,index==3?1:0,-1,nullptr});}
 phase_=2;if(kind_==ScreenControlKind::LiveviewTimeout)return send({ScreenWriteKind::LiveviewTimeout,sourceWanted_,-1,nullptr});if(kind_==ScreenControlKind::AdaptiveFps)return send({ScreenWriteKind::AdaptiveFps,index,-1,nullptr});return sourceWanted_<0?send({ScreenWriteKind::TimeoutZero,0,-1,nullptr}):send({ScreenWriteKind::TimeoutStock,0,sourceWanted_,target_});
}
void ScreenControl::second(){if(state_.backBit!=requestedBack_||state_.evfBit!=requestedEvf_){fail();return;}if(sourceWanted_>=0){bool found=false;for(int i=0;i<count_;i++)if(source_[i]==sourceWanted_&&target_equal(labels_[i]))found=true;if(!found){fail();return;}}phase_=2;if(wanted_==3)send({ScreenWriteKind::ScreensAllowed,state_.backBit|state_.evfBit,-1,nullptr});else send({ScreenWriteKind::DisplayStock,0,sourceWanted_,target_});}
void ScreenControl::tick(uint64_t now){if(dispatching_||!busy_)return;if(now>=deadline_||!valid_||!state_.enabled){fail();return;}
 if(phase_==1){if(state_.evfOnly==(wanted_==3))second();return;}
 bool done=kind_==ScreenControlKind::LiveviewTimeout?(state_.liveviewConfirmed&&state_.liveviewSeconds==sourceWanted_):kind_==ScreenControlKind::AdaptiveFps?state_.fps==(wanted_==1):kind_==ScreenControlKind::Timeout?(sourceWanted_<0?state_.timeoutZero:!state_.timeoutZero&&target_equal(state_.current)):(wanted_==3?state_.evfOnly&&state_.screensAllowed==(state_.backBit|state_.evfBit):!state_.evfOnly&&target_equal(state_.current));
 if(done){busy_=false;phase_=0;complete_=true;}
}
void ScreenControl::close(){if(busy_)fail();valid_=false;}
}
