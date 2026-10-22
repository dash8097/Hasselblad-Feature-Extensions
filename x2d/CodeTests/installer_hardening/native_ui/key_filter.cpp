#include "key_filter.hpp"
#include <new>
namespace x2dui {
// Qt's item key-filter chain does not unlink an attached Keys object on deletion.
// Keep one private attached object for the owner's lifetime; close only our slots.
// This factory address is a distinct Qt cache key, so factory QML Keys stay separate.
// All calls run on the GUI thread; save/restore also permits nested construction.
static const KeyProfile*constructing=nullptr;
static void*createAttached(void*item){if(!constructing)return nullptr;auto p=*constructing;void*o=p.attached(item);if(o)p.complete(o);return o;}
static void property(AbiMetaCall call,void*object,int index,void*value,bool read){int status=-1,flags=0;void*args[]={value,nullptr,&status,&flags};call(object,read?1:2,index,args);}
void NativeKeyFilter::input(void*opaque,void**args){
 auto*s=static_cast<NativeKeyFilter*>(opaque);if(!s->object||!args||!args[1])return;void*e=*static_cast<void**>(args[1]);KeyProfile p=s->profile;
 if(!e||!p.cast(p.meta_event,e))return;KeyInput k;property(p.event,e,0,&k.key,true);property(p.event,e,2,&k.modifiers,true);property(p.event,e,3,&k.repeat,true);
 // Copy everything needed before invoking business code: it may close this
 // filter and its parent page synchronously.
 auto callback=s->handler;void*context=s->context;bool accept=callback(context,k);property(p.event,e,6,&accept,false);
}
void NativeKeyFilter::destroyed(void*p){static_cast<NativeKeyFilter*>(p)->object=nullptr;}
NativeKeyFilter*NativeKeyFilter::create(KeyProfile p,SignalProfile g,void*item,bool(*h)(void*,const KeyInput&),void*c){
 if(!item||!h||!p.cached)return nullptr;auto*s=new(std::nothrow)NativeKeyFilter(p);if(!s)return nullptr;s->handler=h;s->context=c;auto*previous=constructing;constructing=&p;s->object=p.cached(item,createAttached,true);constructing=previous;if(!s->object){delete s;return nullptr;}
 s->death=signal_watch(g,s->object,s->object,"destroyed(QObject*)",destroyed,s,release);s->pressed=signal_watch_args(g,s->object,s->object,"pressed(QQuickKeyEvent*)",input,s,release);
 if(!s->death||!s->pressed){s->close();return nullptr;}return s;
}
void NativeKeyFilter::close(){signal_unwatch(pressed);signal_unwatch(death);pressed=death=nullptr;object=nullptr;delete this;}
}
