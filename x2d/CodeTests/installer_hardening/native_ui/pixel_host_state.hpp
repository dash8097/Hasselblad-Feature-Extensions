#pragma once
#include "pixel_controller.hpp"
namespace x2dui {
enum class ExtensionMode {None,Pixel,Stack};
struct AlbumContext {ProtocolText path;bool contentBrowsing=false,storageBrowsing=false,viewingPhoto=false;};
enum class PixelHostActionKind {ClosePopup,SetStockSingle,RefreshAlbumSamePath,EnterAlbum,UpdateAlbumLoading,FocusReport};
struct PixelHostAction {PixelHostActionKind kind=PixelHostActionKind::ClosePopup;ProtocolText path;};
struct PixelHostState {
 ExtensionMode selected=ExtensionMode::None,requested=ExtensionMode::None;
 bool showing=false,selectionPending=false,applyPending=false,applicationPending=false,preparationPage=false,waitNotice=false;
 UiText applicationError;
 int stackMethod=2,totalSeconds=60,initialDelay=2,targetIso=100;bool noise=false;
};
// p22自有业务协调。Qt树/焦点/代理由外层提供；所有函数均不执行设备I/O。
class PixelHost {
 PixelController&controller_;PixelHostState s_;
 bool oldBusy_=false,oldApplied_=false,oldPreparing_=false,oldPrepared_=false,oldConfirmed_=false;
 ProtocolText oldStage_,beforeAlbum_,refreshTarget_,refreshedOutput_;
 bool storageAvailable_=true,storageSynced_=true,albumNeedsRefresh_=false,albumPending_=false;
 uint64_t deadline_=0,noticeUntil_=0,nextAlbumTry_=0;
 PixelHostAction actions_[16];unsigned actionStart_=0,actionCount_=0;
 void action(PixelHostActionKind,const ProtocolText&path={});
 void remember(const AlbumContext&);
 void open();void perform(uint64_t,const AlbumContext&);
 void refresh_album(uint64_t,const AlbumContext&);
public:
 explicit PixelHost(PixelController&c):controller_(c){}
 const PixelHostState&state()const{return s_;}
 bool select(ExtensionMode);
 bool stack_options(int method,int seconds,bool noise,int delay,int iso);
 bool apply(uint64_t now,const AlbumContext&);
 void observe(uint64_t now,const AlbumContext&);
 bool dismiss();
 bool close_processing();
 void storage_available(bool);
 void storage_synced();
 void queue_album_refresh(const ProtocolText&);
 bool completed_album(const ProtocolText&);
 bool take_action(PixelHostAction&);
 bool album_pending()const{return albumPending_;}
 UiText preparation_text()const;
 static bool native_album(const ProtocolText&);
};
}
