#pragma once
#include "focus_factory.hpp"
#include "focus_popup_view.hpp"
#include "binding_lease.hpp"
#include "dashboard_icons.hpp"
#include "screen_key_profile.hpp"
#include "key_mapping.hpp"
namespace x2dui {
// Borrows the original focus-mode Loader and its setFocusMode(int) signal.
// No Loader source, custom QML, synthetic shutter, or autofocus policy changes.
class FocusAttachment final {
 ViewApi api_;StockProfile stock_;MenuProfile menu_;BindingProfile binding_;SignalProfile signals_;FocusFactoryProfile factory_;FacePopupProfile popup_;FocusPopupStyle style_;
 DashboardIcons dashboard_;int extension_=0;
 FocusModel mode_;FocusSnapshot snapshot_;char16_t heading_[256]{};
 void*root_=nullptr,*control_=nullptr,*model_=nullptr,*loader_=nullptr,*original_=nullptr,*blocked_=nullptr;
 SignalWatch*rootDeath_=nullptr,*controlDeath_=nullptr,*modelDeath_=nullptr,*loaderDeath_=nullptr,*originalDeath_=nullptr,*loaderLoaded_=nullptr;
 BindingLease visible_,enabled_;FocusPopupView*view_=nullptr;
 ScreenKeyProfile keys_{};KeyMapProfile map_{};ScreenKeyLease keyLease_{{},{}};ScreenKeyReceiver*receiver_=nullptr;void*keyItem_=nullptr;SignalWatch*keyDeath_=nullptr;
 struct Watch {FocusAttachment*owner=nullptr;void*object=nullptr;SignalWatch*signals[3]{};Watch*hashNext=nullptr,*freeNext=nullptr;bool seen=false;};
 static constexpr unsigned MaxNodes=4096;Watch watches_[MaxNodes]{};Watch*buckets_[1024]{},*freeWatch_=nullptr;unsigned watchesCount_=0,scans_=0;bool dirty_=true;
 uint64_t now_=0;double width_=0,height_=0;bool running_=false,closed_=false,deferred_=false,touch_=true;unsigned failures_=0;int lastRc_=0;
 static void tree_changed(void*);static void tree_destroyed(void*);bool watch(void*);void clear_watches();void finish_watches();
 static void root_died(void*);static void control_died(void*);static void model_died(void*);static void loader_died(void*);static void original_died(void*);static void key_died(void*);static void release(void*){}
 static void popup_loaded(void*);
 static bool choose(void*,int);static void abort(void*);static bool physical(void*,const KeyInput&,bool);
 bool named(void*,const char16_t*)const;bool state_is_focus()const;bool locate();bool attach(void*);bool refresh();bool choose(int);
 void clear_keys();bool update_keys();void detach(bool notify);void shutdown();void finish_dispatch();
public:
 FocusAttachment(ViewApi,StockProfile,MenuProfile,BindingProfile,SignalProfile,FocusFactoryProfile,FacePopupProfile,FocusPopupStyle);
 ~FocusAttachment(){close();}
 FocusAttachment(const FocusAttachment&)=delete;FocusAttachment&operator=(const FocusAttachment&)=delete;
 bool configure_keys(ScreenKeyProfile,KeyMapProfile);
 int tick(void*root,uint64_t now,double width,double height,bool touchEnabled=true,int extension=0);
 bool key(FocusKey);void close();bool attached()const{return view_&&original_&&model_;}bool has_source()const{return control_&&model_&&loader_;}bool dispatching()const{return running_;}unsigned failures()const{return failures_;}int last_rc()const{return lastRc_;}unsigned scans()const{return scans_;}
};
}
