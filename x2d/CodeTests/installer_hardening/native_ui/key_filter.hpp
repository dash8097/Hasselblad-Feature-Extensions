#pragma once
#include "signal_bridge.hpp"
namespace x2dui {
struct KeyProfile {void*(*attached)(void*);void(*complete)(void*);void(*destroy)(void*);AbiMetaCall event;const void*(*cast)(const void*,const void*);const void*meta_event;void*(*cached)(void*,void*(*)(void*),bool)=nullptr;};
struct KeyInput {int key=0,modifiers=0;bool repeat=false;};
// Returning true consumes the current physical key event; this API never
// synthesizes camera key events or initiates exposure.
class NativeKeyFilter final:private LocalHeap {
 KeyProfile profile;void*object=nullptr;SignalWatch*pressed=nullptr,*death=nullptr;
 bool(*handler)(void*,const KeyInput&)=nullptr;void*context=nullptr;
 explicit NativeKeyFilter(KeyProfile p):profile(p){}
 static void input(void*,void**);static void destroyed(void*);static void release(void*){}
public:
 static NativeKeyFilter*create(KeyProfile,SignalProfile,void*item,bool(*)(void*,const KeyInput&),void*);
 void close();
};
}
