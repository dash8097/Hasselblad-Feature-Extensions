#pragma once
#include "screen_factory_profile.hpp"
#include "screen_key_profile.hpp"
#include "key_mapping.hpp"
#include "screen_mode_session.hpp"
#include "screen_notice_view.hpp"
namespace x2dui {
struct ScreenLiveProfile {
 int(*index)(const void*,const char*)=nullptr;int(*metacall)(void*,int,int,void**)=nullptr;
 void(*point_construct)(void*,void*)=nullptr;void(*point_complete)(void*)=nullptr;
 AbiMetaCall point_property=nullptr;
 unsigned point_size=0;const void*map_type=nullptr;int liveview_active=-1;
};
class ScreenModeAttachment final {
 ViewApi api_;StockProfile stock_;MenuProfile menu_;SignalProfile signals_;ScreenFactoryProfile factory_;ScreenKeyProfile keys_;ScreenLiveProfile live_;KeyMapProfile keyMap_;ScreenKeyLease lease_;ScreenModeSession session_;
 void*scene_=nullptr,*root_=nullptr,*state_=nullptr;ScreenKeyReceiver*receiver_=nullptr;ScreenNoticeView*notice_=nullptr;SignalWatch*sceneDeath_=nullptr,*rootDeath_=nullptr,*stateDeath_=nullptr;
 UiText noticeText_;uint64_t now_=0,nextSearch_=0;unsigned attempts_=0;bool closed_=false,running_=false,closePending_=false;int error_=0;
 LiveviewTimeoutPolicy timeout_;int timeoutSeconds_=0;void*timeoutDrawer_=nullptr,*point_=nullptr,*watchedState_=nullptr;
 SignalWatch*activityWatches_[9]{};SignalWatch*pointDeath_=nullptr;bool timeoutIdle_=false,activity_=false;unsigned timeoutRequests_=0;
 static bool liveview(void*);static bool forward(void*,int);static bool keys(void*,void*,bool);static bool physical(void*,const KeyInput&,bool);
 static void scene_died(void*);static void root_died(void*);static void state_died(void*);static void release(void*){}
 bool read_bool(void*,const char*,bool&);bool ancestor(void*,int&);bool find_state();void*focus();bool prepare(void*);void clear();
 static void activity(void*);static void point_died(void*);bool timeout_hooks();void timeout_tick();bool control_screen();
public:
 ScreenModeAttachment(ViewApi,StockProfile,MenuProfile,SignalProfile,ScreenFactoryProfile,ScreenKeyProfile,ScreenLiveProfile,KeyMapProfile,const char16_t*translatedNotice);
 ~ScreenModeAttachment(){close();}
 ScreenModeAttachment(const ScreenModeAttachment&)=delete;ScreenModeAttachment&operator=(const ScreenModeAttachment&)=delete;
 bool set_notice(const char16_t*text){if(closed_||running_||!text)return false;UiText copy(text);if(!copy.valid()||copy.empty()||copy.size()>511)return false;noticeText_=static_cast<UiText&&>(copy);return true;}
 int tick(void*scene,uint64_t now,double width,double height);void close();bool dispatching()const{return running_||session_.dispatching();}bool intercepting()const{return session_.intercepting();}
 bool hooks_ready()const{return !closed_&&scene_&&root_&&receiver_&&notice_&&state_&&!error_;}
 int error()const{return error_;}bool has_state()const{return state_!=nullptr;}bool closed()const{return closed_;}
 unsigned failures()const{return session_.failures();}bool pending()const{return session_.pending();}
 void configure_timeout(int seconds,void*drawer,bool backendIdle){timeoutSeconds_=seconds;timeoutDrawer_=drawer;timeoutIdle_=backendIdle;}
 unsigned timeout_requests()const{return timeoutRequests_;}
 bool timeout_waiting()const{return timeout_.waiting();}
 bool timeout_hooks_ready()const{return point_&&watchedState_==state_&&activityWatches_[6]&&activityWatches_[7];}
};
}
