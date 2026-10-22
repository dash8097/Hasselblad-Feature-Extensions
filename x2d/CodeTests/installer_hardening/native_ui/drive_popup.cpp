#include "drive_popup.hpp"
namespace x2dui {
ExtensionMode DrivePopup::extension(DriveChoice c){return c==DriveChoice::Pixel?ExtensionMode::Pixel:c==DriveChoice::Stack?ExtensionMode::Stack:ExtensionMode::None;}
const char*DrivePopup::settings(DriveChoice c){switch(c){case DriveChoice::SelfTimer:return "cameraSettingsSelfTimer";case DriveChoice::Interval:return "cameraSettingsInterval";case DriveChoice::ExposureBracketing:return "cameraSettingsExposureBracketing";case DriveChoice::FocusBracketing:return "cameraSettingsFocusBracketing";default:return "";}}
const char16_t*DrivePopup::label(DriveChoice c){static const char16_t*t[]={u"单拍",u"连拍",u"自拍",u"间隔拍摄",u"包围曝光",u"焦点包围",u"像素超频",u"RAW 堆栈"};unsigned i=unsigned(c);return i<8?t[i]:u"";}
const char16_t*DrivePopup::icon(DriveChoice c,bool disabled){
 static const char16_t*t[]={u"image://svg/ic_controlscreen_drive_mode_single_popup",u"image://svg/ic_controlscreen_drive_mode_continuous_popup",u"image://svg/ic_controlscreenpic_drive_mode_selftimer_popup",u"image://svg/ic_controlscreenpic_drive_mode_interval_popup",u"image://svg/ic_controlscreen_drive_mode_exposure_bracketing_popup",u"image://svg/ic_controlscreen_drive_mode_focus_bracketing_popup",u"file:///system/etc/x2d-pixel-overclock/multishot.svg",u"file:///system/etc/x2d-pixel-overclock/stack.svg"};
 if(disabled){if(c==DriveChoice::Interval)return u"image://svg/ic_controlscreenpic_drive_mode_interval_popup_disable";if(c==DriveChoice::ExposureBracketing)return u"image://svg/ic_controlscreen_drive_mode_exposure_bracketing_popup_disable";if(c==DriveChoice::FocusBracketing)return u"image://svg/ic_controlscreen_drive_mode_focus_bracketing_popup_disabled";}return unsigned(c)<8?t[unsigned(c)]:u"";
}
void DrivePopup::configure(DrivePermissions p,DriveChoice actual,ExtensionMode selected){
 permissions_=p;count_=0;for(unsigned i=0;i<8;i++){auto c=DriveChoice(i);if(c==DriveChoice::Pixel&&!p.pixel400&&!p.quarter)continue;if(c==DriveChoice::Stack&&!p.stack)continue;choices_[count_++]=c;}
 for(int i=0;i<count_;i++)if(selected==ExtensionMode::None?choices_[i]==actual:extension(choices_[i])==selected){index_=i;return;}if(index_>=count_)index_=count_-1;
}
bool DrivePopup::selectable(int i)const{if(i<0||i>=count_)return false;auto c=choices_[i];if(extension(c)!=ExtensionMode::None&&!permissions_.manual)return false;if(c==DriveChoice::FocusBracketing&&!permissions_.microFocusAdjust)return false;return !(permissions_.fullAuto&&(c==DriveChoice::Interval||c==DriveChoice::ExposureBracketing||c==DriveChoice::FocusBracketing));}
bool DrivePopup::can_settings(int i)const{return selectable(i)&&(extension(choice(i))!=ExtensionMode::None||settings(choice(i))[0]);}
bool DrivePopup::select_index(int i){if(closed_||!selectable(i))return false;index_=i;return true;}
void DrivePopup::move(int columns,int rows){if(closed_||(!columns&&!rows))return;int delta=rows?(rows>0?2:-2):(columns>0?1:-1);for(int i=index_+delta;i>=0&&i<count_;i+=delta)if(selectable(i)){index_=i;return;}}
bool DrivePopup::enqueue(const DriveAction*a,unsigned n){if(n>16-countActions_)return false;for(unsigned i=0;i<n;i++)actions_[(start_+countActions_+i)%16]=a[i];countActions_+=n;return true;}
bool DrivePopup::select(bool closePopup){
 if(closed_||!selectable(index_))return false;auto c=choice(index_);DriveAction out[5];unsigned n=0;out[n++]={DriveActionKind::SelectExtension,c};
 if(extension(c)!=ExtensionMode::None){if(closePopup)out[n++]={DriveActionKind::ApplyExtension,c};}
 else {out[n++]={DriveActionKind::SetDrive,c};out[n++]={DriveActionKind::SetExitDrive,c};}
 if(closePopup)out[n++]={DriveActionKind::Close,c};if(!enqueue(out,n))return false;if(closePopup){committed_=true;closed_=true;}return true;
}
bool DrivePopup::press(int i){if(closed_||!selectable(i))return false;int old=index_;index_=i;if(select(i==old))return true;index_=old;return false;}
bool DrivePopup::enter(){if(closed_||!selectable(index_))return false;auto c=choice(index_);if(!can_settings(index_))return select(true);DriveAction a{extension(c)==ExtensionMode::None?DriveActionKind::OpenStockSettings:DriveActionKind::OpenExtensionSettings,c};return enqueue(&a,1);}
bool DrivePopup::close(){if(closed_)return true;DriveAction out[3];unsigned n=0;auto c=choice(index_);bool apply=!committed_&&selectable(index_)&&extension(c)!=ExtensionMode::None;if(apply){out[n++]={DriveActionKind::SelectExtension,c};out[n++]={DriveActionKind::ApplyExtension,c};}out[n++]={DriveActionKind::Close,c};if(!enqueue(out,n))return false;committed_=committed_||apply;closed_=true;return true;}
bool DrivePopup::take(DriveAction&a){if(!countActions_)return false;a=actions_[start_];start_=(start_+1)%16;--countActions_;return true;}
}
