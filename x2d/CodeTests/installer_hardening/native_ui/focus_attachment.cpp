#include "focus_attachment.hpp"
#include <cmath>
#include <cstring>
namespace x2dui {namespace {
void release_array(const MenuProfile&m,AbiArray&a,size_t size){if(a.owner){auto*r=static_cast<int*>(a.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)m.deallocate(a.owner,size,8);}}
bool equals(const MenuProfile&m,void*o,const char*property,const char16_t*s){if(!o)return false;auto v=m.property(o,property);auto a=m.string(&v);size_t n=0;while(s[n])++n;bool ok=a.size==std::ptrdiff_t(n)&&(a.data||!n)&&(!n||!std::memcmp(a.data,s,n*2));release_array(m,a,2);m.variant_destroy(&v);return ok;}
bool number_equals(const MenuProfile&m,void*o,const char*n,double expected){if(!o)return false;auto v=m.property(o,n);bool ok=false;double x=m.number(&v,&ok);m.variant_destroy(&v);return ok&&x==expected;}
bool dynamic_int(const StockProfile&s,const MenuProfile&m,void*o,const char*n,int x){alignas(8)unsigned char value[32];s.variant_int(value,x);s.set(o,n,value);s.variant_destroy(value);return number_equals(m,o,n,x);}
}
FocusAttachment::FocusAttachment(ViewApi a,StockProfile s,MenuProfile m,BindingProfile b,SignalProfile g,FocusFactoryProfile f,FacePopupProfile p,FocusPopupStyle style):api_(a),stock_(s),menu_(m),binding_(b),signals_(g),factory_(f),popup_(p),style_(style),dashboard_(s,m,b,g){}
void FocusAttachment::root_died(void*p){auto&s=*static_cast<FocusAttachment*>(p);s.root_=s.control_=s.model_=s.loader_=s.original_=s.blocked_=nullptr;s.dirty_=true;}
void FocusAttachment::control_died(void*p){auto&s=*static_cast<FocusAttachment*>(p);s.control_=nullptr;s.dirty_=true;}
void FocusAttachment::model_died(void*p){auto&s=*static_cast<FocusAttachment*>(p);s.model_=nullptr;s.dirty_=true;}
void FocusAttachment::loader_died(void*p){auto&s=*static_cast<FocusAttachment*>(p);s.loader_=nullptr;s.dirty_=true;}
void FocusAttachment::original_died(void*p){static_cast<FocusAttachment*>(p)->original_=nullptr;}
void FocusAttachment::key_died(void*p){static_cast<FocusAttachment*>(p)->keyItem_=nullptr;}
void FocusAttachment::popup_loaded(void*p){auto&s=*static_cast<FocusAttachment*>(p);if(!s.closed_&&!s.running_&&s.root_&&s.control_&&s.model_&&s.loader_)s.tick(s.root_,s.now_,s.width_,s.height_,s.touch_,s.extension_);}
bool FocusAttachment::named(void*o,const char16_t*n)const{return equals(menu_,o,"objectName",n);}
bool FocusAttachment::state_is_focus()const{return equals(menu_,model_,"mainState",u"focus_mode");}
bool FocusAttachment::configure_keys(ScreenKeyProfile k,KeyMapProfile m){if(closed_||running_||receiver_||!m.instance||!m.meta||!m.cast||!m.is_fn||!m.unblockable||!k.window||!k.active||!k.attached_object||!k.keys_call||!k.key.attached||!k.key.complete||!k.key.event||!k.key.cast||!k.key.meta_event)return false;keys_=k;map_=m;return keyLease_.configure(k,signals_);}
void FocusAttachment::clear_keys(){keyLease_.close();signal_unwatch(keyDeath_);keyDeath_=nullptr;if(receiver_){receiver_->close();receiver_=nullptr;}if(keyItem_){auto*item=keyItem_;keyItem_=nullptr;api_.destroy(api_.context,item);}}
bool FocusAttachment::update_keys(){if(!keys_.window)return true;if(!view_||!original_||!root_){clear_keys();return true;}if(!keyItem_){clear_keys();keyItem_=api_.create(api_.context,Kind::Item,view_->item());if(!keyItem_)return false;keyDeath_=signal_watch(signals_,keyItem_,keyItem_,"destroyed(QObject*)",key_died,this,release);receiver_=ScreenKeyReceiver::create(keys_,signals_,keyItem_,physical,this);if(!keyDeath_||!receiver_){clear_keys();return false;}}
 auto*w=keys_.window(root_);auto*f=w?keys_.active(w):nullptr;return !f||f==keyItem_?keyLease_.close():keyLease_.attach(f,keyItem_);
}
bool FocusAttachment::physical(void*p,const KeyInput&k,bool up){auto&s=*static_cast<FocusAttachment*>(p);if(up||k.repeat||!s.view_||s.closed_||key_unblockable(s.map_,k))return false;auto*o=key_instance(s.map_);if(!o)return false;auto is=[&](int n){return s.map_.is_fn(o,k.key,unsigned(k.modifiers),n);};if(is(27)||is(15))return s.key(FocusKey::Previous);if(is(26)||is(14))return s.key(FocusKey::Next);if(is(28)||is(57))return s.key(FocusKey::Accept);if(is(6)||is(1)||is(61))return s.key(FocusKey::Abort);return false;}
void FocusAttachment::tree_changed(void*p){static_cast<Watch*>(p)->owner->dirty_=true;}
void FocusAttachment::tree_destroyed(void*p){auto*w=static_cast<Watch*>(p);w->object=nullptr;w->owner->dirty_=true;}
void FocusAttachment::clear_watches(){for(unsigned i=0;i<watchesCount_;++i){for(auto*&s:watches_[i].signals){signal_unwatch(s);s=nullptr;}watches_[i].object=nullptr;}watchesCount_=0;freeWatch_=nullptr;for(auto&b:buckets_)b=nullptr;}
void FocusAttachment::finish_watches(){for(unsigned i=0;i<watchesCount_;++i){auto&w=watches_[i];if(!w.seen||!w.object){for(auto*&s:w.signals){signal_unwatch(s);s=nullptr;}w.object=nullptr;}}}
bool FocusAttachment::watch(void*o){auto key=(reinterpret_cast<uintptr_t>(o)>>4)%1024;
 for(auto*w=buckets_[key];w;w=w->hashNext)if(w->object==o){if(w->seen)return false;w->seen=true;return true;}
 auto*w=freeWatch_;if(w)freeWatch_=w->freeNext;else{if(watchesCount_==MaxNodes)return false;w=&watches_[watchesCount_++];}
 for(auto*&s:w->signals){signal_unwatch(s);s=nullptr;}w->owner=this;w->object=o;w->seen=true;w->freeNext=nullptr;w->hashNext=buckets_[key];buckets_[key]=w;
 const char*names[]={"destroyed(QObject*)","childrenChanged()","objectNameChanged(QString)"};for(unsigned i=0;i<3;++i)w->signals[i]=signal_watch(signals_,o,root_,names[i],i?tree_changed:tree_destroyed,w,release);
 return w->signals[0]&&w->signals[1]&&w->signals[2];
}
bool FocusAttachment::locate(){
 // Walk only original visual objects. Duplicate routes and cycles are rejected.
 ++scans_;for(auto&b:buckets_)b=nullptr;freeWatch_=nullptr;for(unsigned i=0;i<watchesCount_;++i){auto&w=watches_[i];w.seen=false;if(w.object){auto k=(reinterpret_cast<uintptr_t>(w.object)>>4)%1024;w.hashNext=buckets_[k];buckets_[k]=&w;}else{w.freeNext=freeWatch_;freeWatch_=&w;}}
 struct Entry{void*p;unsigned depth;};Entry queue[4096];unsigned count=1,visited=0,controls=0,loaders=0;void*c=nullptr,*l=nullptr,*af=nullptr,*drive=nullptr;unsigned afCount=0,driveCount=0;queue[0]={root_,0};
 while(count){auto e=queue[--count];if(!e.p||e.depth>64||visited++==4096)return false;if(view_&&e.p==view_->item())continue;
  // One direct objectName read per node, rather than two QVariant property
  // lookups. Native overlay subtrees cannot contain an original control route.
  auto name=menu_.name(e.p);const auto*text=static_cast<const char16_t*>(name.data);
  bool own=text&&name.size>=9&&!std::memcmp(text,u"X2dNative",18);
  bool control=text&&name.size==24&&!std::memcmp(text,u"ControlScreen_focusScope",48);
  bool loader=text&&name.size==25&&!std::memcmp(text,u"ControlScreen_popupLoader",50);
  if(own){release_array(menu_,name,2);continue;}if(!watch(e.p)){release_array(menu_,name,2);return false;}
  if(text&&name.size==23&&!std::memcmp(text,u"ControlScreen_afControl",46)){af=e.p;++afCount;}
  if(text&&name.size==26&&!std::memcmp(text,u"ControlScreen_driveControl",52)){drive=e.p;++driveCount;}
  release_array(menu_,name,2);if(control){c=e.p;++controls;}if(loader){l=e.p;++loaders;}auto a=menu_.children(e.p);bool ok=a.size>=0&&a.size<=std::ptrdiff_t(4096-count)&&(a.data||!a.size);if(ok)for(std::ptrdiff_t i=0;i<a.size;i++)queue[count++]={static_cast<void**>(a.data)[i],e.depth+1};release_array(menu_,a,8);if(!ok)return false;
 }
 finish_watches();if(controls>1||loaders>1||afCount>1||driveCount>1){dashboard_.close();return false;}if(!c||!l){dashboard_.close();detach(false);signal_unwatch(controlDeath_);signal_unwatch(modelDeath_);signal_unwatch(loaderDeath_);signal_unwatch(loaderLoaded_);loaderLoaded_=nullptr;controlDeath_=modelDeath_=loaderDeath_=nullptr;control_=model_=loader_=nullptr;return true;}if(!menu_contains(menu_,c,l)||(af&&!menu_contains(menu_,c,af))||(drive&&!menu_contains(menu_,c,drive))){dashboard_.close();return false;}dashboard_.targets(af,drive);auto*m=stock_get_object(stock_,c,"viewModel");if(!m)return false;
 if(c==control_&&l==loader_&&m==model_)return !original_||menu_contains(menu_,c,original_);
 detach(false);signal_unwatch(controlDeath_);signal_unwatch(modelDeath_);signal_unwatch(loaderDeath_);signal_unwatch(loaderLoaded_);loaderLoaded_=nullptr;controlDeath_=modelDeath_=loaderDeath_=nullptr;control_=c;model_=m;loader_=l;blocked_=nullptr;
 controlDeath_=signal_watch(signals_,c,c,"destroyed(QObject*)",control_died,this,release);modelDeath_=signal_watch(signals_,m,m,"destroyed(QObject*)",model_died,this,release);loaderDeath_=signal_watch(signals_,l,l,"destroyed(QObject*)",loader_died,this,release);
 signal_unwatch(loaderLoaded_);loaderLoaded_=signal_watch(signals_,l,l,"loaded()",popup_loaded,this,release);
 return controlDeath_&&modelDeath_&&loaderDeath_&&loaderLoaded_;
}
bool FocusAttachment::attach(void*o){if(!o||o==blocked_||!control_||!model_||!named(o,u"PopoverFocusMode_root")||!menu_contains(menu_,control_,o))return false;original_=o;originalDeath_=signal_watch(signals_,o,o,"destroyed(QObject*)",original_died,this,release);
 if(!originalDeath_||!visible_.acquire(stock_,binding_,signals_,o,u"visible")||!enabled_.acquire(stock_,binding_,signals_,o,u"enabled")){detach(false);return false;}
 mode_=FocusModel{};view_=FocusPopupView::create(api_,stock_,signals_,popup_,o,control_,choose,abort,this);
 if(!view_||closed_||!original_||!dynamic_int(stock_,menu_,view_->item(),"keyOverride",2)||!dynamic_int(stock_,menu_,view_->item(),"guiPopupState",4)||!refresh()||!visible_.number(0)||!enabled_.number(0)){detach(false);return false;}
 return !closed_&&original_&&model_;
}
bool FocusAttachment::refresh(){if(closed_||!view_||!view_->item()||!original_||!model_||!state_is_focus()||!named(original_,u"PopoverFocusMode_root"))return false;
 if(!focus_factory_snapshot(factory_,stock_,menu_,original_,model_,snapshot_,heading_,256))return false;mode_.observe(snapshot_,now_);FocusPopupState s;s.source=snapshot_;s.selected=mode_.selected();s.pending=mode_.pending();s.failed=mode_.failed();s.closed=mode_.closed();style_.heading=heading_;return view_->update(s,style_,width_,height_,touch_);
}
void FocusAttachment::detach(bool notify){auto*old=original_;bool canNotify=notify&&old&&loader_&&model_&&stock_get_object(stock_,loader_,"item")==old&&state_is_focus();clear_keys();if(view_){view_->close();view_=nullptr;}
 bool va=visible_.alive(),ea=enabled_.alive();bool v=visible_.close(),e=enabled_.close();if((va&&!v)||(ea&&!e)){++failures_;blocked_=old;}
 signal_unwatch(originalDeath_);originalDeath_=nullptr;original_=nullptr;mode_=FocusModel{};
 if(canNotify&&model_&&loader_&&stock_get_object(stock_,loader_,"item")==old&&state_is_focus()){blocked_=old;if(!stock_set_text(stock_,model_,"mainState",u""))++failures_;}
}
bool FocusAttachment::choose(void*p,int n){return static_cast<FocusAttachment*>(p)->choose(n);}
bool FocusAttachment::choose(int n){if(closed_||running_||!view_||!original_)return false;running_=true;bool accepted=false;
 auto*expected=original_;
 // Structural events reconcile the cache; every explicit write intent validates
 // the current unique route again; a removed/replaced route cannot use cache.
 if(locate()&&!closed_&&view_&&original_==expected&&loader_&&stock_get_object(stock_,loader_,"item")==expected&&refresh()&&!closed_&&original_&&model_){int value=0;auto action=mode_.choose(n,now_,value);accepted=action!=FocusAction::None;if(action==FocusAction::SetMode){if(!focus_factory_select(factory_,stock_,original_,value)){mode_.reject_pending();++failures_;}}if(!closed_&&view_&&original_&&model_&&!refresh()){++failures_;}}
 finish_dispatch();return accepted;
}
void FocusAttachment::abort(void*p){auto&s=*static_cast<FocusAttachment*>(p);if(!s.closed_)s.mode_.abort();}
bool FocusAttachment::key(FocusKey k){if(closed_||running_||!view_)return false;bool result=view_->key(k);if(!closed_&&!running_&&view_&&!update_keys())++failures_;return result;}
void FocusAttachment::finish_dispatch(){running_=false;if(deferred_)shutdown();}
int FocusAttachment::tick(void*root,uint64_t now,double w,double h,bool touch,int extension){if(closed_||running_)return 1;if(!root||now<now_||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||w>16384||h>16384)return 2;
 running_=true;extension_=extension;now_=now;width_=w;height_=h;touch_=touch;int rc=0;
 if(root_!=root){dashboard_.close();detach(false);clear_watches();signal_unwatch(controlDeath_);signal_unwatch(modelDeath_);signal_unwatch(loaderDeath_);signal_unwatch(loaderLoaded_);loaderLoaded_=nullptr;signal_unwatch(rootDeath_);controlDeath_=modelDeath_=loaderDeath_=rootDeath_=nullptr;control_=model_=loader_=nullptr;blocked_=nullptr;dirty_=true;root_=root;rootDeath_=signal_watch(signals_,root,root,"destroyed(QObject*)",root_died,this,release);if(!rootDeath_)rc=3;}
 if(!rc&&(dirty_||(control_&&stock_get_object(stock_,control_,"viewModel")!=model_))){dirty_=false;if(!locate())rc=4;}
 if(!rc&&model_&&!dashboard_.update(model_,extension_)){++failures_;lastRc_=9;}
 auto*item=loader_?stock_get_object(stock_,loader_,"item"):nullptr;if(!item)blocked_=nullptr;
 if(!rc&&item!=original_){detach(false);if(item!=blocked_)blocked_=nullptr;if(item&&item!=blocked_&&state_is_focus()&&named(item,u"PopoverFocusMode_root")&&!attach(item))rc=5;}
 if(!rc&&view_){if(!state_is_focus())detach(false);else if(!visible_.number(0)||!enabled_.number(0)){blocked_=original_;detach(false);rc=6;}else if(mode_.closed()||view_->finished())detach(true);else if(!refresh()){detach(false);rc=7;}else if(mode_.closed()||view_->finished())detach(true);}
 if(!closed_&&!rc&&!update_keys())rc=8;if(rc){++failures_;lastRc_=rc;blocked_=item;detach(false);}finish_dispatch();return rc;
}
void FocusAttachment::shutdown(){dashboard_.close();deferred_=false;detach(false);clear_watches();signal_unwatch(loaderLoaded_);loaderLoaded_=nullptr;signal_unwatch(rootDeath_);signal_unwatch(controlDeath_);signal_unwatch(modelDeath_);signal_unwatch(loaderDeath_);rootDeath_=controlDeath_=modelDeath_=loaderDeath_=nullptr;root_=control_=model_=loader_=blocked_=nullptr;}
void FocusAttachment::close(){if(closed_)return;closed_=true;deferred_=true;if(!running_)shutdown();}
}
