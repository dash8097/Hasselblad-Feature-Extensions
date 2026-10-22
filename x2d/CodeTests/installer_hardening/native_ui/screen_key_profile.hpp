#pragma once
#include "key_filter.hpp"
namespace x2dui {
struct ScreenKeyList {void*object=nullptr,*data=nullptr;void(*append)(void*,void*)=nullptr;std::ptrdiff_t(*count)(void*)=nullptr;void*(*at)(void*,std::ptrdiff_t)=nullptr;void(*clear)(void*)=nullptr;void(*replace)(void*,std::ptrdiff_t,void*)=nullptr;void(*removeLast)(void*)=nullptr;};
struct ScreenKeyProfile {KeyProfile key;void*(*attached_object)(void*,void*(*)(void*),bool)=nullptr;AbiMetaCall keys_call=nullptr;unsigned listIndex=1;void*(*window)(const void*)=nullptr;void*(*active)(const void*)=nullptr;void*(*parent_item)(const void*)=nullptr;};
// 仅GUI线程；保留当前 foreign targets 的顺序，只增删 ownTarget。owner/target销毁后不回写旧对象。
class ScreenKeyLease final {
 ScreenKeyProfile profile_;SignalProfile signals_;void*owner_=nullptr,*target_=nullptr,*attached_=nullptr;SignalWatch*ownerDeath_=nullptr,*targetDeath_=nullptr,*attachedDeath_=nullptr;bool active_=false;
 static void owner_died(void*);static void target_died(void*);static void attached_died(void*);static void release(void*){}
 bool list(ScreenKeyList&);bool mutate(bool);void clear_watches();
public:
 ScreenKeyLease(ScreenKeyProfile p,SignalProfile s):profile_(p),signals_(s){}
 ~ScreenKeyLease(){close();}
 ScreenKeyLease(const ScreenKeyLease&)=delete;ScreenKeyLease&operator=(const ScreenKeyLease&)=delete;
 bool attach(void*owner,void*ownTarget);bool close();bool valid()const{return active_&&owner_&&target_&&attached_;}
 bool configure(ScreenKeyProfile p,SignalProfile s){if(!close())return false;profile_=p;signals_=s;return true;}
};
// 仅给本扩展自己创建的Item挂接；pressed/released只消费物理事件，不合成按键。
class ScreenKeyReceiver final:private LocalHeap {
 ScreenKeyProfile profile_;void*attached_=nullptr;SignalWatch*press_=nullptr,*release_=nullptr,*death_=nullptr;
 bool(*handler_)(void*,const KeyInput&,bool)=nullptr;void*context_=nullptr;
 explicit ScreenKeyReceiver(ScreenKeyProfile p):profile_(p){}
 static void pressed(void*,void**);static void released(void*,void**);static void died(void*);static void dispose(void*){}
 void input(void**,bool);
public:
 static ScreenKeyReceiver*create(ScreenKeyProfile,SignalProfile,void*ownItem,bool(*)(void*,const KeyInput&,bool),void*);
 void close();
};
}
