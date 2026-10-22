#include "screen_key_profile.hpp"
#include <new>
namespace x2dui {
static_assert(sizeof(ScreenKeyList)==64,"Qt6 QQmlListProperty fixed ABI");
namespace {void property(AbiMetaCall call,void*o,int n,void*v,bool read){int status=-1,flags=0;void*a[]={v,nullptr,&status,&flags};call(o,read?1:2,n,a);}}
void ScreenKeyLease::owner_died(void*p){auto*s=static_cast<ScreenKeyLease*>(p);s->owner_=s->attached_=nullptr;s->active_=false;}
void ScreenKeyLease::target_died(void*p){auto*s=static_cast<ScreenKeyLease*>(p);s->close();}
void ScreenKeyLease::attached_died(void*p){auto*s=static_cast<ScreenKeyLease*>(p);s->attached_=nullptr;s->active_=false;}
void ScreenKeyLease::clear_watches(){signal_unwatch(ownerDeath_);signal_unwatch(targetDeath_);signal_unwatch(attachedDeath_);ownerDeath_=targetDeath_=attachedDeath_=nullptr;}
bool ScreenKeyLease::list(ScreenKeyList&l){if(!owner_||!attached_||!profile_.keys_call)return false;property(profile_.keys_call,attached_,int(profile_.listIndex),&l,true);return l.object==attached_&&l.count&&l.at&&l.clear&&l.append;}
bool ScreenKeyLease::mutate(bool add){ScreenKeyList l;if(!list(l))return false;auto n=l.count(&l);if(n<0)return false;
 if(!add){if(!l.replace||!l.removeLast)return false;for(std::ptrdiff_t i=n;i>0;){--i;if(l.at(&l,i)!=target_)continue;for(std::ptrdiff_t j=i;j+1<n;j++)l.replace(&l,j,l.at(&l,j+1));l.removeLast(&l);--n;}return l.count(&l)==n;}
 if(n>64)return false;void*keep[65]{};size_t size=0;keep[size++]=target_;for(std::ptrdiff_t i=0;i<n;i++){void*p=l.at(&l,i);if(p!=target_)keep[size++]=p;}if(size>64)return false;
 // 固定Qt list回调不派发事件；不运行任意QML setter。GUI线程无跨线程修改窗口。
 l.clear(&l);for(size_t i=0;i<size;i++)l.append(&l,keep[i]);if(l.count(&l)!=std::ptrdiff_t(size))return false;for(size_t i=0;i<size;i++)if(l.at(&l,std::ptrdiff_t(i))!=keep[i])return false;return true;
}
bool ScreenKeyLease::attach(void*owner,void*target){if(valid()&&owner_==owner&&target_==target){bool enabled=false;property(profile_.keys_call,attached_,0,&enabled,true);ScreenKeyList l;if(enabled&&list(l)){auto n=l.count(&l);for(std::ptrdiff_t i=0;i<n;i++)if(l.at(&l,i)==target_)return true;}}if(!close()||!owner||!target||owner==target||!profile_.attached_object||!profile_.key.attached||!profile_.key.complete||!profile_.keys_call)return false;
 owner_=owner;target_=target;ownerDeath_=signal_watch(signals_,owner,owner,"destroyed(QObject*)",owner_died,this,release);targetDeath_=signal_watch(signals_,target,target,"destroyed(QObject*)",target_died,this,release);if(!ownerDeath_||!targetDeath_){close();return false;}
 attached_=profile_.attached_object(owner,profile_.key.attached,false);bool made=!attached_;if(made)attached_=profile_.attached_object(owner,profile_.key.attached,true);if(!attached_){close();return false;}attachedDeath_=signal_watch(signals_,attached_,attached_,"destroyed(QObject*)",attached_died,this,release);if(!attachedDeath_){close();return false;}if(made)profile_.key.complete(attached_);bool enabled=false;property(profile_.keys_call,attached_,0,&enabled,true);if(!enabled){close();return false;}active_=true;if(!mutate(true)){close();return false;}return true;
}
bool ScreenKeyLease::close(){bool ok=true;if(active_&&owner_&&attached_)ok=mutate(false);active_=false;clear_watches();owner_=target_=attached_=nullptr;return ok;}
void ScreenKeyReceiver::pressed(void*p,void**a){static_cast<ScreenKeyReceiver*>(p)->input(a,false);}void ScreenKeyReceiver::released(void*p,void**a){static_cast<ScreenKeyReceiver*>(p)->input(a,true);}void ScreenKeyReceiver::died(void*p){static_cast<ScreenKeyReceiver*>(p)->attached_=nullptr;}
void ScreenKeyReceiver::input(void**args,bool released){if(!attached_||!args||!args[1])return;void*e=*static_cast<void**>(args[1]);auto p=profile_.key;if(!e||!p.cast(p.meta_event,e))return;KeyInput k;property(p.event,e,0,&k.key,true);property(p.event,e,2,&k.modifiers,true);property(p.event,e,3,&k.repeat,true);auto callback=handler_;void*c=context_;bool accepted=callback(c,k,released);property(p.event,e,6,&accepted,false);}
ScreenKeyReceiver*ScreenKeyReceiver::create(ScreenKeyProfile p,SignalProfile g,void*item,bool(*h)(void*,const KeyInput&,bool),void*c){if(!item||!h||!p.attached_object||!p.key.attached||!p.key.complete||!p.key.event||!p.key.cast||!p.key.meta_event)return nullptr;
 // 新建owned Item不应已有Keys；拒绝借用并接管别人的handler。
 if(p.attached_object(item,p.key.attached,false))return nullptr;auto*s=new(std::nothrow)ScreenKeyReceiver(p);if(!s)return nullptr;s->handler_=h;s->context_=c;s->attached_=p.attached_object(item,p.key.attached,true);if(!s->attached_){delete s;return nullptr;}
 s->death_=signal_watch(g,s->attached_,s->attached_,"destroyed(QObject*)",died,s,dispose);s->press_=signal_watch_args(g,s->attached_,s->attached_,"pressed(QQuickKeyEvent*)",pressed,s,dispose);s->release_=signal_watch_args(g,s->attached_,s->attached_,"released(QQuickKeyEvent*)",released,s,dispose);if(!s->death_||!s->press_||!s->release_){s->close();return nullptr;}p.key.complete(s->attached_);return s;
}
void ScreenKeyReceiver::close(){signal_unwatch(press_);signal_unwatch(release_);signal_unwatch(death_);press_=release_=death_=nullptr;attached_=nullptr;delete this;}
}
