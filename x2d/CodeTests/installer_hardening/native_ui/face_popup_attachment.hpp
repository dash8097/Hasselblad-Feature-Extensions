#pragma once
#include "face_factory.hpp"
#include "binding_lease.hpp"
#include "screen_key_profile.hpp"
#include "key_mapping.hpp"
namespace x2dui {
// Fixed original popup Loader, borrowed proxies; never changes Loader.source.
class FacePopupAttachment final {
 ViewApi api_;StockProfile stock_;MenuProfile menu_;BindingProfile binding_;SignalProfile signals_;FaceFactoryProfile factory_;FacePopupProfile popupProfile_;FacePopupStyle style_;
 FaceModeModel mode_;FaceActions actions_;FacePopupSnapshot snapshot_;char16_t heading_[256]{};
 void*scene_=nullptr,*loader_=nullptr,*original_=nullptr,*blocked_=nullptr;SignalWatch*sceneDeath_=nullptr,*loaderDeath_=nullptr,*originalDeath_=nullptr;
 BindingLease visible_,enabled_;FacePopupView*view_=nullptr;
 ScreenKeyProfile keys_{};KeyMapProfile map_{};ScreenKeyLease keyLease_{{},{}};ScreenKeyReceiver*receiver_=nullptr;void*keyItem_=nullptr;SignalWatch*keyDeath_=nullptr;
 uint64_t now_=0,nextSearch_=0,nextMode_=0,epoch_=0;unsigned attempts_=0,failures_=0;double width_=0,height_=0;bool running_=false,closed_=false,deferred_=false,touch_=true;
 static void scene_died(void*);static void loader_died(void*);static void original_died(void*);static void key_died(void*);static void release(void*){}
 static bool choose(void*,int);static bool physical(void*,const KeyInput&,bool);
 bool named(void*,const char16_t*)const;bool find_loader();bool refresh();bool choose(int);void drain();bool attach(void*);void detach(bool notify);void shutdown();
 bool update_keys();void clear_keys();
public:
 FacePopupAttachment(ViewApi,StockProfile,MenuProfile,BindingProfile,SignalProfile,FaceFactoryProfile,FacePopupProfile,FacePopupStyle);
 ~FacePopupAttachment(){close();}
 FacePopupAttachment(const FacePopupAttachment&)=delete;FacePopupAttachment&operator=(const FacePopupAttachment&)=delete;
 bool configure_keys(ScreenKeyProfile,KeyMapProfile);
 int tick(void*scene,uint64_t now,double width,double height,bool touchEnabled=true);
 bool key(FacePopupKey);void close();bool dispatching()const{return running_;}bool attached()const{return view_&&original_;}bool has_source()const{return loader_;}unsigned failures()const{return failures_;}
};
}
