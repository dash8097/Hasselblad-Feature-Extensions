#pragma once
#include "effect_view_core.hpp"
#include <cstddef>
namespace x2dui {
// Fixed Qt 6.4.1 ARM64 layout, separately checked against original meta tables.
struct AbiString {void*owner;const char16_t*data;std::ptrdiff_t size;};
// QString constructed by the existing Qt runtime owns/copies its UTF-16 data.
// A null-owner aggregate would instead act like fromRawData and may be retained.
template<class P>inline AbiString abi_owned_string(const P&p,const char16_t*data,std::ptrdiff_t size){AbiString s{};p.string_ctor(&s,data,size);return s;}
template<class P>inline void abi_release_string(const P&p,AbiString&s){if(s.owner){auto*r=static_cast<int*>(s.owner);if(__atomic_load_n(r,__ATOMIC_RELAXED)>=0&&__atomic_sub_fetch(r,1,__ATOMIC_ACQ_REL)==0)p.string_deallocate(s.owner,2,8);}s={nullptr,nullptr,0};}
struct AbiStringView {const char*data;std::size_t size;};
struct AbiColor {unsigned spec;unsigned short alpha,red,green,blue,pad;};
struct AbiPoint {double x,y;};
struct AbiConnection {void*data;~AbiConnection(){};}; // Non-trivial => AArch64 indirect result x8.
struct AbiSlot;
using AbiMetaCall=void(*)(void*,int,int,void**);
struct AbiViewProfile {
 void*(*allocate)(std::size_t);void(*construct[4])(void*,void*);void(*destroy)(void*);
 unsigned sizes[4];void(*parent)(void*,void*);void(*complete[4])(void*);
 AbiMetaCall item,rectangle,text,mouse_event;
 void(*set_name)(void*,AbiStringView);void(*font_ctor)(void*);void(*font_size)(void*,int);void(*font_dtor)(void*);
 AbiPoint(*map_scene)(const void*,const AbiPoint&);
 AbiConnection(*connect)(const void*,void**,const void*,void**,AbiSlot*,int,const int*,const void*);
 void(*connection_dtor)(AbiConnection*);
 const void*meta_object;const void*meta_item;const void*meta_mouse;
 void*signal_functions[6]; // clicked, pressed, positionChanged, released, destroyed, visibleChanged
 void(*string_ctor)(void*,const char16_t*,std::ptrdiff_t);void(*string_deallocate)(void*,std::ptrdiff_t,std::ptrdiff_t);
 AbiPoint(*map_from_scene)(const void*,const AbiPoint&)=nullptr;
};
ViewApi abi_view_api(AbiViewProfile*);
}
