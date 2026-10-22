#include "signal_bridge.hpp"
#include <cstdlib>
namespace x2dui {
struct SignalWatch {SignalProfile profile;void*connection;};
struct Slot {int refs;void(*impl)(int,Slot*,void*,void**,bool*);void(*callback)(void*);void*context;void(*release)(void*);void(*arguments)(void*,void**);};
static_assert(sizeof(AbiMethod)==16&&offsetof(Slot,impl)==8,"Qt method/slot ABI");
static void invoke(int op,Slot*s,void*,void**args,bool*equal){if(op==0){if(s->release)s->release(s->context);std::free(s);}else if(op==1){if(s->arguments)s->arguments(s->context,args);else s->callback(s->context);}else if(op==2&&equal)*equal=false;}
static SignalWatch*watch(const SignalProfile&p,void*sender,void*receiver,const char*signature,void(*callback)(void*),void(*arguments)(void*,void**),void*context,void(*release)(void*),int*error){
 if(error)*error=2;
 if(!sender||!receiver||!signature||(!callback&&!arguments)||!release)return nullptr;
 // Itanium QObject ABI: the first virtual is metaObject(). The fixed-image
 // adapter validates both vtable and function mappings before this dispatch.
 const void*table=*static_cast<const void**>(sender);if(!table)return nullptr;
 const void*fn=*static_cast<const void*const*>(table);
 if(p.valid_vtable&&!p.valid_vtable(p.validation_context,table,fn))return nullptr;
 const void*meta=reinterpret_cast<const void*(*)(const void*)>(reinterpret_cast<std::uintptr_t>(fn))(sender);if(!meta)return nullptr;
 int index=p.index(meta,signature);if(index<0){if(error)*error=1;return nullptr;}AbiMethod method=p.method(meta,index);int signal=p.signal_index(method);if(signal<0)return nullptr;
 // QObjectPrivate::connect takes the method index and performs the signal-index conversion itself.
 auto*w=static_cast<SignalWatch*>(std::calloc(1,sizeof(SignalWatch)));auto*s=static_cast<Slot*>(std::calloc(1,sizeof(Slot)));if(!w||!s){std::free(w);std::free(s);return nullptr;}
 *s={1,invoke,callback,context,nullptr,arguments};AbiConnection connection=p.connect(sender,index,receiver,s,1);
 if(!connection.data){p.connection_dtor(&connection);std::free(w);return nullptr;}
 if(error)*error=0;s->release=release;w->profile=p;w->connection=connection.data;return w;
}
SignalWatch*signal_watch(const SignalProfile&p,void*s,void*r,const char*n,void(*f)(void*),void*c,void(*d)(void*),int*e){return watch(p,s,r,n,f,nullptr,c,d,e);}
SignalWatch*signal_watch_args(const SignalProfile&p,void*s,void*r,const char*n,void(*f)(void*,void**),void*c,void(*d)(void*),int*e){return watch(p,s,r,n,nullptr,f,c,d,e);}
void signal_unwatch(SignalWatch*w){if(!w)return;AbiConnection connection{w->connection};w->profile.disconnect(connection);w->profile.connection_dtor(&connection);std::free(w);}
}
