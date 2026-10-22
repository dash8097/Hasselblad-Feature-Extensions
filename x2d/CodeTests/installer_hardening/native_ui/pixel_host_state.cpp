#include "pixel_host_state.hpp"
#include <limits>
namespace x2dui {
bool PixelHost::native_album(const ProtocolText&s){
 if(!s.valid())return false;if(s=="/ssd")return true;if(s.size()!=13||std::memcmp(s.data(),"/ssd/",5)||s[5]<'1'||s[5]>'9')return false;
 return s[6]>='0'&&s[6]<='9'&&s[7]>='0'&&s[7]<='9'&&!std::memcmp(s.data()+8,"HASBL",5);
}
void PixelHost::action(PixelHostActionKind kind,const ProtocolText&path){
 if(actionCount_==16){s_.applicationError=u"界面动作未及时处理";return;}auto&slot=actions_[(actionStart_+actionCount_)%16];slot={kind,path};if(!slot.path.valid()){s_.applicationError=u"界面内存不足";return;}++actionCount_;
}
bool PixelHost::take_action(PixelHostAction&out){if(!actionCount_)return false;out=static_cast<PixelHostAction&&>(actions_[actionStart_]);actionStart_=(actionStart_+1)%16;--actionCount_;return true;}
void PixelHost::remember(const AlbumContext&a){if(native_album(a.path))beforeAlbum_=a.path;albumNeedsRefresh_=true;}
void PixelHost::queue_album_refresh(const ProtocolText&target){if(!target.valid())return;if(!target.empty()){if(!native_album(target))return;ProtocolText copy(target);if(!copy.valid())return;refreshTarget_=static_cast<ProtocolText&&>(copy);}albumNeedsRefresh_=albumPending_=true;}
void PixelHost::storage_available(bool available){storageAvailable_=available;if(!available){storageSynced_=false;albumNeedsRefresh_=true;}}
void PixelHost::storage_synced(){storageSynced_=true;if(albumNeedsRefresh_)queue_album_refresh({});}
bool PixelHost::completed_album(const ProtocolText&location){
 if(!location.valid()||controller_.state().busy||location==refreshedOutput_||(location.size()!=21&&location.size()!=22))return false;
 if(location[0]<'1'||location[0]>'9'||location[1]<'0'||location[1]>'9'||location[2]<'0'||location[2]>'9'||std::memcmp(location.data()+3,"HASBL/",6)||location[9]<'A'||location[9]>'Z')return false;
 for(size_t i=10;i<17;i++)if(location[i]<'0'||location[i]>'9')return false;
 if(!((location.size()==21&&!std::memcmp(location.data()+17,".JPG",4))||(location.size()==22&&!std::memcmp(location.data()+17,".HEIC",5))))return false;
 ProtocolText target("/ssd/");target.append(location.data(),8);ProtocolText output(location);
 if(!target.valid()||!output.valid())return false;
 refreshTarget_=static_cast<ProtocolText&&>(target);refreshedOutput_=static_cast<ProtocolText&&>(output);albumNeedsRefresh_=albumPending_=true;return true;
}
void PixelHost::refresh_album(uint64_t now,const AlbumContext&a){
 if(!albumPending_||now<nextAlbumTry_)return;nextAlbumTry_=now+200;auto&c=controller_.state();
 if(!storageAvailable_||!storageSynced_||c.busy||c.preparingMode||a.contentBrowsing||a.storageBrowsing||a.viewingPhoto)return;
 auto target=refreshTarget_.empty()?a.path:refreshTarget_;if(!target.valid())return;if(!native_album(target))target=beforeAlbum_;if(!target.valid())return;
 if(!native_album(target)){albumPending_=false;return;}
 if(actionCount_>14)return; // 两个相关动作必须共同入队。
 PixelHostAction refresh{a.path==target?PixelHostActionKind::RefreshAlbumSamePath:PixelHostActionKind::EnterAlbum,target};
 if(!refresh.path.valid())return;
 actions_[(actionStart_+actionCount_)%16]=static_cast<PixelHostAction&&>(refresh);
 actions_[(actionStart_+actionCount_+1)%16]={PixelHostActionKind::UpdateAlbumLoading,{}};actionCount_+=2;
 albumPending_=albumNeedsRefresh_=false;refreshTarget_.clear();
}
bool PixelHost::select(ExtensionMode mode){
 if(mode!=ExtensionMode::None&&mode!=ExtensionMode::Pixel&&mode!=ExtensionMode::Stack)return false;
 s_.selected=mode;s_.selectionPending=mode!=ExtensionMode::None;
 if(mode==ExtensionMode::None){s_.applicationError.clear();s_.applicationPending=s_.applyPending=false;controller_.disarm_mode();}return true;
}
bool PixelHost::stack_options(int method,int seconds,bool noise,int delay,int iso){
 (void)noise;
 if(controller_.state().busy||method<0||method>2||seconds<1||seconds>86399||delay<0||delay>60||iso<1||iso>65535)return false;
 s_.stackMethod=method;s_.totalSeconds=seconds;s_.noise=false;s_.initialDelay=delay;s_.targetIso=iso;return true;
}
void PixelHost::open(){action(PixelHostActionKind::ClosePopup);s_.showing=true;}
bool PixelHost::dismiss(){if(controller_.state().busy)return false;bool owned=s_.showing;s_.showing=false;if(owned)action(PixelHostActionKind::FocusReport);return true;}
bool PixelHost::close_processing(){if(controller_.state().busy)return false;if(s_.preparationPage){controller_.disarm_mode();s_.preparationPage=false;}return dismiss();}
bool PixelHost::apply(uint64_t now,const AlbumContext&a){
 if(controller_.state().exposureMode!=0){s_.applicationError=u"请切换到 M 档后使用";return false;}
 if(controller_.state().busy||s_.selected==ExtensionMode::None||now>std::numeric_limits<uint64_t>::max()-2500)return false;
 if(actionCount_>13){s_.applicationError=u"界面动作未及时处理";return false;}
 s_.requested=s_.selected;s_.applicationError.clear();s_.selectionPending=s_.applicationPending=true;deadline_=now+2500;
 controller_.tick(now);observe(now,a);return true;
}
void PixelHost::perform(uint64_t,const AlbumContext&a){
 // Readiness may arrive later, after unrelated UI actions filled the queue.
 // Reserve all required quarter/close/focus actions before changing the controller.
 if(actionCount_>13){s_.applyPending=false;s_.applicationError=u"界面动作未及时处理";s_.showing=true;return;}
 auto&c=controller_.state();if(!(s_.selected==ExtensionMode::Pixel&&c.resolution==25))remember(a);
 s_.applyPending=s_.selected==ExtensionMode::Stack?controller_.apply_stack(s_.stackMethod,s_.totalSeconds,s_.noise,s_.initialDelay,s_.targetIso):controller_.apply_pixel();
 if(!s_.applyPending){s_.applicationError=c.lastError.empty()?UiText(u"模式应用失败"):c.lastError;open();return;}
 s_.selectionPending=false;
 if(c.quarter_mode()){action(PixelHostActionKind::SetStockSingle);action(PixelHostActionKind::ClosePopup);s_.preparationPage=s_.applyPending=false;dismiss();return;}
 if(c.preparingMode&&!s_.preparationPage){if(s_.showing)dismiss();s_.preparationPage=true;action(PixelHostActionKind::ClosePopup);}
}
void PixelHost::observe(uint64_t now,const AlbumContext&a){
 auto&c=controller_.state();
 if(s_.applicationPending){
  if(c.exposureMode!=0){s_.applicationPending=s_.applyPending=false;s_.applicationError=u"请切换到 M 档后重新选择模式";}
  else if(c.busy||s_.requested!=s_.selected)s_.applicationPending=false;
  else if(now>=deadline_){s_.applicationPending=false;s_.applicationError=u"模式应用失败：拍摄后端未就绪";open();}
  else if(s_.requested==ExtensionMode::Pixel&&c.resolution==25?controller_.application_ready():c.can_start()&&c.nativeReady){s_.applicationPending=false;perform(now,a);}
 }
 if(c.preparingMode!=oldPreparing_){if(c.preparingMode)remember(a);else s_.waitNotice=false;}
 if(c.preparedMode&&!oldPrepared_&&!c.quarter_mode())queue_album_refresh({});
 if(c.stage!=oldStage_&&!c.stackMode&&c.resolution==25&&c.stage=="failed")open();
 if(c.busy&&!oldBusy_){s_.selectionPending=s_.preparationPage=s_.applyPending=false;s_.selected=c.stackMode?ExtensionMode::Stack:ExtensionMode::Pixel;if(c.stackMode||c.resolution!=25)open();}
 if(c.applied!=oldApplied_){
  if(c.applied)s_.selected=c.stackMode?ExtensionMode::Stack:ExtensionMode::Pixel;else if(!s_.selectionPending)s_.selected=ExtensionMode::None;
  if(!c.applied){s_.applyPending=false;if(s_.preparationPage){s_.preparationPage=false;if(c.stage=="failed")open();else dismiss();}}
 }
 bool confirmed=controller_.applied_confirmed();if(confirmed&&!oldConfirmed_&&s_.preparationPage){s_.applyPending=s_.preparationPage=false;dismiss();action(PixelHostActionKind::FocusReport);}
 PixelEvent event;while(controller_.take_event(event)){
  if(event.kind==PixelEventKind::PreparationWait){if(c.applied&&c.preparingMode){s_.waitNotice=true;noticeUntil_=now+2000;}}
  else {s_.selectionPending=false;completed_album(event.album);s_.selected=c.keepMode?ExtensionMode::Pixel:ExtensionMode::None;s_.applyPending=false;dismiss();}
 }
 if(s_.waitNotice&&now>=noticeUntil_)s_.waitNotice=false;
 oldPreparing_=c.preparingMode;oldPrepared_=c.preparedMode;oldBusy_=c.busy;oldApplied_=c.applied;oldConfirmed_=confirmed;oldStage_=c.stage;
 refresh_album(now,a);
}
UiText PixelHost::preparation_text()const{
 const auto&c=controller_.state();if(!s_.preparationPage||!c.preparingMode||c.busy)return {};
 return c.shutterPending?u"正在准备，完成后自动拍摄":s_.waitNotice?u"正在准备，请稍后":u"正在准备";
}
}
