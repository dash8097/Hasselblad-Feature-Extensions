#pragma once
#include "screen_factory_profile.hpp"
#include "screen_controls_view.hpp"
#include "binding_lease.hpp"
#include "screen_key_profile.hpp"
#include "key_mapping.hpp"
namespace x2dui {
struct LiveviewControlApi {void*context=nullptr;bool(*read)(void*,int&,bool&)=nullptr;bool(*write)(void*,int,uint64_t)=nullptr;};
// GUI线程。真实菜单Loader下替换三个指定行；不写Loader.source，不运行自有QML。
class ScreenAttachment final {
 struct Row {
  ScreenAttachment*owner;ScreenControlKind kind;ScreenControl model;ScreenFactorySnapshot snapshot;
  void*loader=nullptr,*original=nullptr,*blockedOriginal=nullptr;BindingLease visible,enabled,height;ScreenControlView*view=nullptr;
  Row(ScreenAttachment*,ScreenControlKind);static bool apply(void*,const ScreenWrite&);static bool choose(void*,int);
 } rows_[3];
 LiveviewControlApi liveviewApi_{};ScreenControl liveviewModel_;ScreenControlView*liveviewView_=nullptr;
 UiText liveviewTitle_;UiText liveviewLabels_[7];bool liveviewLower_=false,liveviewSelected_=false;int lastDirection_=1;
 static bool liveview_apply(void*,const ScreenWrite&);static bool liveview_choose(void*,int);bool refresh_liveview(Row&,double,double,double,double);bool timeout_selected()const;
 struct Watch {ScreenAttachment*owner=nullptr;void*object=nullptr;SignalWatch*signals[7]{};Watch*hashNext=nullptr,*freeNext=nullptr;bool seen=false;};
 ViewApi api_;StockProfile stock_;AbiViewProfile*abi_;MenuProfile menu_;BindingProfile binding_;SignalProfile signals_;ScreenFactoryProfile factory_;ScreenControlStyle style_;
 ScreenKeyProfile keyProfile_{};KeyMapProfile keyMap_{};ScreenKeyLease keyLease_{{},{}};ScreenKeyReceiver*keyReceiver_=nullptr;void*keyItem_=nullptr;SignalWatch*keyDeath_=nullptr;
 void*scene_=nullptr,*drawer_=nullptr;SignalWatch*sceneDeath_=nullptr;static constexpr unsigned MaxNodes=4096;Watch watches_[MaxNodes]{};Watch*buckets_[1024]{},*freeWatch_=nullptr;unsigned watchesCount_=0,attempts_=0,scans_=0,connections_=0;
 uint64_t now_=0,nextSearch_=0;double width_=0,height_=0;bool dirty_=true,active_=false,touch_=true,running_=false,closed_=false,closePending_=false;int error_=0;
 static void changed(void*);static void destroyed(void*);static void sceneDestroyed(void*);static void release(void*){}
 bool named(void*,const char16_t*)const;bool owned(void*)const;bool read_number(void*,const char*,double&)const;bool read_text(void*,const char*,UiText&)const;
 void clear_watches();void detach(Row&);void detach_rows();bool watch(void*);bool scan(void*);bool refresh(Row&);bool attach(Row&,void*,void*);
 bool dispatch(Row&,const ScreenWrite&);bool choose(Row&,int);void shutdown();
 static bool physical(void*,const KeyInput&,bool);static void key_died(void*);bool update_keys();void clear_keys();
public:
 ScreenAttachment(ViewApi,StockProfile,AbiViewProfile*,MenuProfile,BindingProfile,SignalProfile,ScreenFactoryProfile,ScreenControlStyle);
 ~ScreenAttachment(){close();}
 ScreenAttachment(const ScreenAttachment&)=delete;ScreenAttachment&operator=(const ScreenAttachment&)=delete;
 int tick(void*freshRoot,uint64_t now,double width,double height,bool touchEnabled=true);
 bool configure_keys(ScreenKeyProfile,KeyMapProfile);
 bool configure_liveview(LiveviewControlApi,const char16_t*,const char16_t*const*);
 void*drawer()const{return drawer_;}
 bool key(ScreenControlKey);void close();bool dispatching()const noexcept{return running_;}
 bool hooks_ready()const{return !closed_&&scene_&&sceneDeath_&&drawer_&&!error_;}
 unsigned subscriptionConnections()const noexcept{return connections_;}
 unsigned scans()const noexcept{return scans_;}int error()const noexcept{return error_;}unsigned attached()const noexcept;
};
}
