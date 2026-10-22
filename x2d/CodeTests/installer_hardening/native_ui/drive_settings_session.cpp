#include "drive_settings_session.hpp"
#include <limits>
namespace x2dui {
bool DriveSettingsSession::allowed(DriveChoice c,DrivePermissions p)const{
 if(unsigned(c)>unsigned(DriveChoice::Stack))return false;
 if(DrivePopup::extension(c)!=ExtensionMode::None&&(!p.manual||controller_.state().exposureMode!=0))return false;
 if(c==DriveChoice::Pixel)return (features_.pixel400&&p.pixel400)||(features_.quarter&&p.quarter);
 if(c==DriveChoice::Stack)return features_.rawStack&&p.stack;
 if(c==DriveChoice::FocusBracketing&&!p.microFocusAdjust)return false;
 return !(p.fullAuto&&(c==DriveChoice::Interval||c==DriveChoice::ExposureBracketing||c==DriveChoice::FocusBracketing));
}
bool DriveSettingsSession::dispatch(const DriveAction*a,unsigned n,uint64_t now,const AlbumContext&album,DrivePermissions permissions){
 if(!a||!n||n>5||pending()||now>std::numeric_limits<uint64_t>::max()-5000||nextId_>std::numeric_limits<uint64_t>::max()-5)return false;
 const bool closeOnly=n==1&&a[0].kind==DriveActionKind::Close;
 if(!closeOnly&&(controller_.state().busy||controller_.state().sending))return false;
 auto choice=a[0].choice;if(unsigned(choice)>unsigned(DriveChoice::Stack)||(!closeOnly&&!allowed(choice,permissions)))return false;
 for(unsigned i=0;i<n;i++)if(a[i].choice!=choice)return false;
 auto extension=DrivePopup::extension(choice);bool valid=false;
 if(n==1){valid=a[0].kind==DriveActionKind::Close||(a[0].kind==DriveActionKind::SelectExtension&&extension!=ExtensionMode::None)||(a[0].kind==DriveActionKind::OpenExtensionSettings&&extension!=ExtensionMode::None)||(a[0].kind==DriveActionKind::OpenStockSettings&&DrivePopup::settings(choice)[0]);}
 else if(a[0].kind==DriveActionKind::SelectExtension){
  if(extension!=ExtensionMode::None)valid=n==3&&a[1].kind==DriveActionKind::ApplyExtension&&a[2].kind==DriveActionKind::Close;
  else valid=(n==3||n==4)&&a[1].kind==DriveActionKind::SetDrive&&a[2].kind==DriveActionKind::SetExitDrive&&(n==3||a[3].kind==DriveActionKind::Close);
 }
 if(!valid)return false;
 for(unsigned i=0;i<n;i++)actions_[i]=a[i];count_=n;next_=0;pending_={};offered_=normalChecked_=false;deadline_=now+5000;error_=u"";status_=DriveSessionStatus::Pending;
 tick(now,album);return true;
}
void DriveSettingsSession::fail(const char16_t*error){status_=DriveSessionStatus::Failed;error_=error;pending_={};offered_=false;count_=next_=0;}
bool DriveSettingsSession::enqueue(DriveSessionIntentKind kind,DriveChoice c){if(!nextId_){fail(u"界面会话已失效");return false;}pending_={nextId_++,kind,c};offered_=false;return true;}
void DriveSettingsSession::tick(uint64_t now,const AlbumContext&album){
 if(!pending())return;if(now>=deadline_){fail(u"模式切换结果未确认，请查看诊断");return;}
 if(next_==count_&&!pending_.id){status_=DriveSessionStatus::Complete;return;}
 if(controller_.state().busy&&!(next_<count_&&actions_[next_].kind==DriveActionKind::Close)){fail(u"拍摄进行中，模式切换已停止");return;}
 if(pending_.id)return;
 while(next_<count_){const auto action=actions_[next_];const auto extension=DrivePopup::extension(action.choice);
  if(extension!=ExtensionMode::None&&action.kind!=DriveActionKind::Close&&controller_.state().exposureMode!=0){fail(u"请切换到 M 档后使用");return;}
  switch(action.kind){
  case DriveActionKind::SelectExtension:if(!host_.select(extension)){fail(u"模式选择失败");return;}++next_;break;
  case DriveActionKind::ApplyExtension:if(!host_.apply(now,album)){fail(u"模式应用未接受，请查看诊断");return;}++next_;break;
  case DriveActionKind::SetDrive:if(!normalChecked_){enqueue(DriveSessionIntentKind::AwaitNormalMode,action.choice);return;}enqueue(DriveSessionIntentKind::SetDrive,action.choice);return;
  case DriveActionKind::SetExitDrive:enqueue(DriveSessionIntentKind::SetExitDrive,action.choice);return;
  case DriveActionKind::OpenStockSettings:enqueue(DriveSessionIntentKind::OpenStockSettings,action.choice);return;
  case DriveActionKind::OpenExtensionSettings:page_=extension;++next_;break;
  case DriveActionKind::Close:enqueue(DriveSessionIntentKind::ClosePopup,action.choice);return;
  }
 }
 status_=DriveSessionStatus::Complete;
}
bool DriveSettingsSession::take_intent(DriveSessionIntent&out,uint64_t now){if(!pending()||!pending_.id||offered_)return false;
 if(now>=deadline_){fail(u"模式切换结果未确认，请查看诊断");return false;}
 bool setter=pending_.kind==DriveSessionIntentKind::SetDrive||pending_.kind==DriveSessionIntentKind::SetExitDrive;
 if((controller_.state().busy&&pending_.kind!=DriveSessionIntentKind::ClosePopup)||(setter&&controller_.state().sending)){fail(u"相机状态已变化，模式切换已停止");return false;}
 out=pending_;offered_=true;return true;}
bool DriveSettingsSession::complete(uint64_t id,DriveSessionResult result,uint64_t now){
 if(!pending()||!offered_||!id||pending_.id!=id)return false;
 if(now>=deadline_){fail(u"模式切换结果未确认，请查看诊断");return false;}
 if(result==DriveSessionResult::Confirmed&&pending_.kind!=DriveSessionIntentKind::ClosePopup&&(controller_.state().busy||controller_.state().sending)){fail(u"相机状态已变化，模式切换结果未确认");return true;}
 if(result!=DriveSessionResult::Confirmed){fail(result==DriveSessionResult::Rejected?u"相机未接受模式切换，请查看诊断":u"模式切换结果未确认，请查看诊断");return true;}
 if(pending_.kind==DriveSessionIntentKind::AwaitNormalMode)normalChecked_=true;else ++next_;
 pending_={};offered_=false;return true;
}
void DriveSettingsSession::cancel(){if(pending()){status_=DriveSessionStatus::Cancelled;error_=offered_?u"操作已停止，已发送步骤的结果可能未确认":u"";}pending_={};offered_=false;count_=next_=0;page_=ExtensionMode::None;}
void DriveSettingsSession::leave(SettingsExit e){if(e!=SettingsExit::None)page_=ExtensionMode::None;}
bool DriveSettingsSession::change(const SettingsChange&v){
 const auto&c=controller_.state();if(pending()||page_==ExtensionMode::None||c.busy||c.sending)return false;
 if(page_==ExtensionMode::Pixel){if(v.alsoTargetIso)return false;switch(v.option){
 case SettingsOption::Resolution:return controller_.set_option("pixelResolution",v.value);
 case SettingsOption::PixelDelay:return controller_.set_option("initialDelay",v.value);
 case SettingsOption::Keep:return controller_.set_option("keepMode",v.value);
 default:return false;}}
 const auto&s=host_.state();int method=s.stackMethod,seconds=s.totalSeconds,delay=s.initialDelay,iso=s.targetIso;bool noise=false;
 if(v.alsoTargetIso)return false;
 switch(v.option){
 case SettingsOption::Method:method=v.value;break;
 case SettingsOption::TotalSeconds:seconds=v.value;break;
 case SettingsOption::StackDelay:delay=v.value;break;
 case SettingsOption::TargetIso:return false;
 case SettingsOption::Noise:return false;
 default:return false;}
 return host_.stack_options(method,seconds,noise,delay,iso);
}
SettingsSnapshot DriveSettingsSession::snapshot(const UiText&aperture,const UiText&shutter,const UiText&iso)const{
 const auto&c=controller_.state();const auto&h=host_.state();SettingsSnapshot s;
 s.controller=true;s.busy=pending()||c.busy||c.sending;s.pixel400=features_.pixel400;s.quarter=features_.quarter;s.stack=page_==ExtensionMode::Stack;
 s.resolution=c.resolution;s.pixelDelay=c.initialDelay;s.keep=c.keepMode;s.method=h.stackMethod;s.totalSeconds=h.totalSeconds;s.stackDelay=h.initialDelay;s.targetIso=h.targetIso;s.noise=h.noise;
 s.applied=c.applied;s.controllerIso=c.stack_iso_mode();s.controllerTargetIso=c.stackTargetIso;s.controllerPlannedFrames=c.stackPlannedFrames;
 // 预览新方式不能混入上一种堆栈的结果，尤其不能显示26/2帧。
 bool same=c.stackMode&&c.stackMethod==h.stackMethod&&
           (h.stackMethod==3?c.stackTargetIso==h.targetIso:c.stackSeconds==h.totalSeconds);
 s.frames=same?c.stackFrameCount:0;s.gapSeconds=same?c.stackGapMs/1000.:-1;s.plannedFrames=same?c.stackPlannedFrames:0;
 if(same&&c.stack_iso_mode()&&c.stackFrameCount>0){s.outputIso=double(c.stackSourceIso)/c.stackFrameCount;if(c.stackShutterDenominator)s.outputSeconds=double(c.stackShutterNumerator)*c.stackFrameCount/c.stackShutterDenominator;}
 s.aperture=aperture;s.shutter=shutter;s.iso=iso;s.outputShutter=controller_.shutter_text();return s;
}
}
