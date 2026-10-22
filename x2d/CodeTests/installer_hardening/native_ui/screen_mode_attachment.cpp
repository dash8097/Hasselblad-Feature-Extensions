#include "screen_mode_attachment.hpp"
#include <cmath>
#include <cstring>
#include <new>
namespace x2dui {namespace {
void releaseArray(const MenuProfile&p,AbiArray&a,size_t n){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.deallocate(a.owner,n,8);}}
bool named(const MenuProfile&p,void*o,const char16_t*n){auto a=p.name(o);size_t len=0;while(n[len])len++;bool ok=a.data&&a.size==std::ptrdiff_t(len)&&!memcmp(a.data,n,len*2);releaseArray(p,a,2);return ok;}
}
ScreenModeAttachment::ScreenModeAttachment(ViewApi a,StockProfile s,MenuProfile m,SignalProfile sig,ScreenFactoryProfile f,ScreenKeyProfile k,ScreenLiveProfile l,KeyMapProfile map,const char16_t*t):api_(a),stock_(s),menu_(m),signals_(sig),factory_(f),keys_(k),live_(l),keyMap_(map),lease_(k,sig),session_({this,liveview,forward,keys}),noticeText_(t){}
bool ScreenModeAttachment::read_bool(void*o,const char*n,bool&b){if(!o||!stock_has(stock_,o,n))return false;auto v=menu_.property(o,n);bool ok=false;double x=menu_.number(&v,&ok);menu_.variant_destroy(&v);if(!ok||(x!=0&&x!=1))return false;b=x!=0;return true;}
void*ScreenModeAttachment::focus(){void*w=scene_&&keys_.window?keys_.window(scene_):nullptr;void*p=w&&keys_.active?keys_.active(w):nullptr;return p==root_?nullptr:p;}
bool ScreenModeAttachment::ancestor(void*o,int&value){value=0;void*seen[64]{};unsigned n=0;while(o){if(n==64||!keys_.parent_item)return false;for(unsigned i=0;i<n;i++)if(seen[i]==o)return false;seen[n++]=o;if(o!=root_&&stock_has(stock_,o,"keyOverride")){auto v=menu_.property(o,"keyOverride");bool ok=false;double x=menu_.number(&v,&ok);menu_.variant_destroy(&v);if(!ok||!std::isfinite(x)||x<0||x>255||x!=int(x))return false;if(x){value=int(x);return true;}}o=keys_.parent_item(o);}return true;}
bool ScreenModeAttachment::forward(void*p,int value){auto*s=static_cast<ScreenModeAttachment*>(p);return factory_write(s->factory_.proxy,FactoryField::ForwardInput,value);}
bool ScreenModeAttachment::keys(void*p,void*owner,bool attach){auto*s=static_cast<ScreenModeAttachment*>(p);return attach?s->root_&&s->lease_.attach(owner,s->root_):s->lease_.close();}
bool ScreenModeAttachment::liveview(void*p){auto*s=static_cast<ScreenModeAttachment*>(p);if(s->closed_||!s->state_||!s->live_.index||!s->live_.metacall||!s->stateDeath_)return false;
 const void*table=*static_cast<const void**>(s->state_);if(!table)return false;const void*fn=*static_cast<const void*const*>(table);if(s->stock_.valid_vtable&&!s->stock_.valid_vtable(s->stock_.validation_context,table,fn))return false;const void*meta=reinterpret_cast<const void*(*)(const void*)>(reinterpret_cast<uintptr_t>(fn))(s->state_);int index=s->live_.index(meta,"setLiveView(bool)");if(index<0)return false;bool enabled=true;void*args[]={nullptr,&enabled};return s->live_.metacall(s->state_,0,index,args)<0;
}
bool ScreenModeAttachment::physical(void*p,const KeyInput&k,bool released){auto*s=static_cast<ScreenModeAttachment*>(p);if(s->closed_||s->running_)return false;bool half=key_matches(s->keyMap_,k,KeyFunction::HalfPress);return released?s->session_.release(half):s->session_.press(half,k.repeat,s->now_);}
void ScreenModeAttachment::scene_died(void*p){auto*s=static_cast<ScreenModeAttachment*>(p);s->scene_=nullptr;s->close();}
void ScreenModeAttachment::root_died(void*p){static_cast<ScreenModeAttachment*>(p)->root_=nullptr;}
void ScreenModeAttachment::state_died(void*p){auto*s=static_cast<ScreenModeAttachment*>(p);s->state_=s->watchedState_=nullptr;s->activity_=true;}
void ScreenModeAttachment::activity(void*p){static_cast<ScreenModeAttachment*>(p)->activity_=true;}
void ScreenModeAttachment::point_died(void*p){static_cast<ScreenModeAttachment*>(p)->point_=nullptr;}
bool ScreenModeAttachment::timeout_hooks(){
 if(!live_.point_construct||!live_.point_complete||!live_.point_property||live_.point_size!=32||!live_.map_type||live_.liveview_active<0||!root_||!state_)return false;
 if(watchedState_!=state_){for(auto*&w:activityWatches_){signal_unwatch(w);w=nullptr;}watchedState_=nullptr;
  const char*names[]={"keyPressed(int)","customButton(int,bool)","customWheel(int,bool)","focusModeChanged()"};
  bool ok=true;for(unsigned i=0;i<4;i++){activityWatches_[i]=signal_watch(signals_,state_,root_,names[i],activity,this,release);ok=ok&&activityWatches_[i];}
  auto&p=factory_.proxy;void*camera=p.slots[0]?*p.slots[0]:nullptr;
  if(!camera||!p.cast(p.meta[0],camera))return false;
  activityWatches_[4]=signal_watch(signals_,camera,root_,"lens_ring_rotationChanged(bool)",activity,this,release);
  activityWatches_[5]=signal_watch(signals_,camera,root_,"focus_pointChanged(uint)",activity,this,release);
  if(!ok||!activityWatches_[4]||!activityWatches_[5])return false;
  watchedState_=state_;activity_=true;
 }
 if(!point_){signal_unwatch(activityWatches_[6]);signal_unwatch(activityWatches_[7]);activityWatches_[6]=activityWatches_[7]=nullptr;signal_unwatch(pointDeath_);pointDeath_=nullptr;point_=stock_.allocate(live_.point_size);if(!point_)return false;
  live_.point_construct(point_,root_);pointDeath_=signal_watch(signals_,point_,root_,"destroyed(QObject*)",point_died,this,release);
  if(!pointDeath_){auto*p=point_;point_=nullptr;stock_.object_destroy(p);return false;}
  void*target=nullptr;void*args[]={&target};live_.point_property(point_,2,2,args);
  live_.point_complete(point_);
 }
 if(!activityWatches_[6])activityWatches_[6]=signal_watch(signals_,point_,root_,"pointChanged()",activity,this,release);
 if(!activityWatches_[7])activityWatches_[7]=signal_watch(signals_,point_,root_,"activeChanged()",activity,this,release);
 return activityWatches_[6]&&activityWatches_[7]&&pointDeath_;
}
bool ScreenModeAttachment::control_screen(){
 if(!state_||!stateDeath_||!live_.map_type)return false;
 const void*table=*static_cast<const void**>(state_);if(!table)return false;const void*fn=*static_cast<const void*const*>(table);
 if(stock_.valid_vtable&&!stock_.valid_vtable(stock_.validation_context,table,fn))return false;
 const void*meta=reinterpret_cast<const void*(*)(const void*)>(reinterpret_cast<uintptr_t>(fn))(state_);
 int index=live_.index(meta,"toggle_screen(QString,QVariant)");if(index<0)return false;
 auto text=abi_owned_string(stock_,u"control_screen",14);void*emptyMap=nullptr;alignas(8)unsigned char argument[32];
 stock_.variant_typed(argument,{live_.map_type},&emptyMap);void*args[]={nullptr,&text,argument};
 bool ok=live_.metacall(state_,0,index,args)<0;stock_.variant_destroy(argument);abi_release_string(stock_,text);return ok;
}
void ScreenModeAttachment::timeout_tick(){
 bool eligible=timeoutSeconds_>0&&timeoutIdle_&&timeoutDrawer_&&timeout_hooks(),touching=false;
 auto number=[&](void*o,const char*n,double&value){if(!o||!stock_has(stock_,o,n))return false;auto v=menu_.property(o,n);bool ok=false;value=menu_.number(&v,&ok);menu_.variant_destroy(&v);return ok&&std::isfinite(value);};
 if(eligible){
  auto v=menu_.property(timeoutDrawer_,"mainState");auto a=menu_.string(&v);eligible=a.size==8&&a.data&&!memcmp(a.data,u"liveview",16);releaseArray(menu_,a,2);menu_.variant_destroy(&v);
  double status=-1;eligible=eligible&&number(state_,"liveviewState",status)&&status==live_.liveview_active&&number(state_,"exposureStatus",status)&&status==0;
  const char*blocked[]={"inSession","activeError","isShutdown","isUpgrading","isFormatting","isMaintenanceManual","isMaintenanceUpgrade","isTethered"};
  for(const char*name:blocked){bool value=true;eligible=read_bool(state_,name,value)&&!value&&eligible;}
  auto&p=factory_.proxy;void*cam=p.slots[0]?*p.slots[0]:nullptr;void*sys=p.slots[1]?*p.slots[1]:nullptr;void*storage=p.slots[2]?*p.slots[2]:nullptr;
  bool ring=true,processing=true,visible=false;
  eligible=eligible&&cam&&sys&&storage&&p.cast(p.meta[0],cam)&&p.cast(p.meta[1],sys)&&p.cast(p.meta[2],storage)&&read_bool(cam,"lens_ring_rotation",ring)&&!ring&&read_bool(sys,"media_processing",processing)&&!processing&&number(storage,"storage_status",status)&&status==0;
  void*window=keys_.window(scene_);eligible=eligible&&read_bool(window,"visible",visible)&&visible&&read_bool(point_,"active",touching);
 }
 bool expired=timeout_.update(now_,timeoutSeconds_,eligible,touching,activity_);activity_=false;
 if(expired&&control_screen())++timeoutRequests_;
}
bool ScreenModeAttachment::find_state(){if(state_)return true;if(now_<nextSearch_||attempts_>=80)return true;++attempts_;nextSearch_=now_>UINT64_MAX-250?UINT64_MAX:now_+250;void*pending[512],*seen[512];unsigned n=1,count=0,hits=0;pending[0]=scene_;void*found=nullptr;while(n){void*o=pending[--n];if(!o||count==512)return false;for(unsigned i=0;i<count;i++)if(seen[i]==o)return false;seen[count++]=o;if(o==root_)continue;if(named(menu_,o,u"StateMachineViewModel_root")){found=o;++hits;}auto a=menu_.children(o);bool valid=a.size>=0&&a.size<=512-n&&(a.data||!a.size);if(valid){auto**items=static_cast<void**>(a.data);for(std::ptrdiff_t i=0;i<a.size;i++)pending[n++]=items[i];}releaseArray(menu_,a,8);if(!valid)return false;}if(hits>1)return false;if(found){signal_unwatch(stateDeath_);stateDeath_=signal_watch(signals_,found,found,"destroyed(QObject*)",state_died,this,release);if(!stateDeath_)return false;state_=found;}return true;}
bool ScreenModeAttachment::prepare(void*scene){if(scene_)return scene_==scene&&root_&&receiver_&&notice_;if(!noticeText_.valid()||noticeText_.size()>511||!keys_.window||!keys_.active||!keys_.parent_item)return false;scene_=scene;sceneDeath_=signal_watch(signals_,scene,scene,"destroyed(QObject*)",scene_died,this,release);if(!sceneDeath_)return false;root_=api_.create(api_.context,Kind::Item,scene);if(!root_)return false;api_.name(api_.context,root_,"X2dNativeScreenMode");api_.layer(api_.context,root_,2000,false);rootDeath_=signal_watch(signals_,root_,root_,"destroyed(QObject*)",root_died,this,release);receiver_=ScreenKeyReceiver::create(keys_,signals_,root_,physical,this);notice_=ScreenNoticeView::create(api_,stock_,scene,root_);return rootDeath_&&receiver_&&notice_;}
int ScreenModeAttachment::tick(void*scene,uint64_t now,double w,double h){if(closed_||running_)return 1;if(!scene||now<now_||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||w>16384||h>16384)return 2;running_=true;now_=now;error_=0;if(!prepare(scene)||!find_state())error_=3;
 ScreenModeSnapshot s;int64_t evf=0,allowed=0,active=0,forward=0;if(!error_&&(!factory_read(factory_.proxy,FactoryField::EvfOnly,evf)||!factory_read(factory_.proxy,FactoryField::ScreensAllowed,allowed)||!factory_read(factory_.proxy,FactoryField::ScreensActive,active)||!factory_read(factory_.proxy,FactoryField::ForwardInput,forward)||forward<0||forward>255))error_=4;
 if(!error_){s.mode.evfOnly=evf!=0;s.mode.backAllowed=(allowed&factory_.backBit)!=0;s.mode.evfAllowed=(allowed&factory_.evfBit)!=0;s.mode.backActive=(active&factory_.backBit)!=0;s.mode.evfActive=(active&factory_.evfBit)!=0;s.mode.modelPresent=state_!=nullptr;if(state_&&(!read_bool(state_,"screenEVF",s.mode.modelEvf)||!read_bool(state_,"isShutdown",s.mode.shutdown)||!read_bool(state_,"isMaintenanceUpgrade",s.mode.upgrade)||!read_bool(state_,"isMaintenanceManual",s.mode.maintenance)||!read_bool(state_,"exposing",s.mode.exposing)))error_=5;s.focusOwner=focus();if(!ancestor(s.focusOwner,s.ancestorOverride))error_=6;s.forward=int(forward);s.halfOverrideBit=factory_.halfBit;}
 if(!error_){if(session_.intercepting()&&!lease_.valid())session_.invalidate_keys();session_.update(s);if(!closed_&&root_&&notice_){api_.rect(api_.context,root_,0,0,w,h);if(!notice_->update(session_.notice(now_),w,h,noticeText_.data()))error_=7;}}else{ScreenModeSnapshot inactive;s.focusOwner=focus();ancestor(s.focusOwner,inactive.ancestorOverride);session_.update(inactive);if(notice_)notice_->update(false,w,h,noticeText_.data());if(error_==3)closed_=closePending_=true;}
 if(!error_&&!closed_)timeout_tick();else timeout_.update(now_,0,false,false,false);
 running_=false;if(closePending_)clear();return error_;
}
void ScreenModeAttachment::clear(){closePending_=false;for(auto*&w:activityWatches_){signal_unwatch(w);w=nullptr;}signal_unwatch(pointDeath_);pointDeath_=nullptr;watchedState_=nullptr;if(point_){auto*p=point_;point_=nullptr;stock_.object_destroy(p);}session_.close();lease_.close();signal_unwatch(stateDeath_);signal_unwatch(sceneDeath_);signal_unwatch(rootDeath_);stateDeath_=sceneDeath_=rootDeath_=nullptr;state_=scene_=nullptr;if(receiver_){receiver_->close();receiver_=nullptr;}if(notice_){notice_->close();notice_=nullptr;}if(root_){auto*p=root_;root_=nullptr;api_.destroy(api_.context,p);}}
void ScreenModeAttachment::close(){if(closed_)return;closed_=closePending_=true;int value=0;if(scene_&&ancestor(focus(),value))session_.close_with_override(value);else session_.close();if(!dispatching())clear();}
}
