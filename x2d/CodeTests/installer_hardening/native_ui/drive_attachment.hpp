#pragma once
#include "drive_settings_session.hpp"
#include "drive_popup_view.hpp"
#include "multishot_settings_view.hpp"
#include "processing_host.hpp"
#include "binding_lease.hpp"
#include "tree_observer.hpp"
namespace x2dui {
struct DriveAttachmentProfile {int(*method_index)(const void*,const char*)=nullptr;int(*metacall)(void*,int,int,void**)=nullptr;int drive[6]{},fullAuto=0,halfOverride=0;int keyUp[2]{},keyDown[2]{},keyLeft[2]{},keyRight[2]{},keyAccept[2]{},keyParameters=0;};
class DriveSettingsAttachment {
 StockProfile stock;MenuProfile menu;AbiViewProfile*abi;SignalProfile signals;BindingProfile binding;KeyProfile keys;KeyMapProfile keymap;FocusProfile focus;DriveAttachmentProfile original;ViewApi api;
 PixelController&controller;PixelHost&host;DriveSettingsSession session;
 DrivePopupView*popup=nullptr;MultishotSettingsView*settings=nullptr;NativeKeyFilter*key=nullptr;
 void*scene=nullptr,*control=nullptr,*model=nullptr,*factoryPopup=nullptr,*grid=nullptr,*swipe=nullptr,*summaryList=nullptr;
 SignalWatch*deaths[6]{};BindingLease visibility,enabled;bool lost=false,stockPage=false,ownershipLost=false;
 SignalWatch*sceneDeath=nullptr;
 // Only the original control screen owns this popup. Its stable lifetime
 // replaces subscriptions to every unrelated object in the application tree.
 void*sourceControl=nullptr,*sourceLoader=nullptr;SignalWatch*sourceDeath=nullptr,*loaderDeath=nullptr,*loaderLoaded=nullptr;bool inTick=false;
 SignalWatch*returnWatch=nullptr;void*returnFocus=nullptr;
 uint64_t nextDiscovery=0;uint64_t now=0;AlbumContext album;DrivePermissions permissions;
 DriveStyle driveStyle;SettingsStyle settingsStyle;double width=0,height=0;bool touch=true;
 DriveSessionIntent hostIntent{};uint64_t hostNext=uint64_t(1)<<63,hostDeadline=0;bool hostOffered=false;const char16_t*hostError=u"";
 static void died(void*);static void release(void*){}
 static void sceneDied(void*);
 static void sourceDied(void*);void clearSource();bool discoverSource(void*,uint64_t);
 static void loaderDied(void*);static void popupLoaded(void*);
 static void focusDied(void*);
 static bool dispatch(void*,const DriveAction*,unsigned);static bool summary(void*,DriveChoice,DriveSummary&);
 static bool changed(void*,const SettingsChange&);static void left(void*,SettingsExit);static bool pressed(void*,const KeyInput&);
 bool locate(void*);bool attach(void*);void detach();bool showSettings();bool callStockSettings(DriveChoice);bool collectRows(void*,DriveSummary&,unsigned&,unsigned depth=0);
 bool serviceUiIntent(const DriveSessionIntent&);bool updateViews();bool named(void*,const char16_t*)const;
 bool findSummary(void*,void*&,unsigned&,unsigned&,unsigned depth=0);
 void*currentModel()const;
public:
 DriveSettingsAttachment(StockProfile,MenuProfile,AbiViewProfile*,SignalProfile,BindingProfile,KeyProfile,KeyMapProfile,FocusProfile,DriveAttachmentProfile,PixelController&,PixelHost&,PixelFeatures={});
 DriveSettingsAttachment(const DriveSettingsAttachment&)=delete;
 DriveSettingsAttachment&operator=(const DriveSettingsAttachment&)=delete;
 ~DriveSettingsAttachment(){close();}
 bool tick(void*freshRoot,uint64_t,const AlbumContext&,const DriveStyle&,const SettingsStyle&,double,double);
 // Only original camera-proxy intentions leave this class. Adapter confirms
 // current readback, never an enqueue result. UI-only intents execute locally.
 bool take_intent(DriveSessionIntent&out,uint64_t at);
 bool complete(uint64_t id,DriveSessionResult result,uint64_t at);
 // Acceptance is queue ownership only. SetStockSingle emits one SetDrive(Single)
 // for the existing original-proxy adapter; no synthetic hardware success.
 bool dispatch_host_action(const PixelHostAction&,uint64_t at);
 bool attached()const{return popup&&!lost;}
 const char16_t*error()const{return hostError[0]?hostError:session.error();}
 void close();void*item()const{return settings?settings->item():popup?popup->item():nullptr;}
};
}
