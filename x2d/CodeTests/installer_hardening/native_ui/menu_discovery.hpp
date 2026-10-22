#pragma once
#include "abi_view_api.hpp"
namespace x2dui {
struct AbiArray {void*owner=nullptr;void*data=nullptr;std::ptrdiff_t size=0;~AbiArray(){}};
struct AbiVariant {alignas(8) unsigned char data[32];~AbiVariant(){}};
struct MenuProfile {
 AbiArray(*children)(const void*);AbiArray(*name)(const void*);
 AbiVariant(*property)(const void*,const char*);AbiArray(*string)(const AbiVariant*);double(*number)(const AbiVariant*,bool*);void(*variant_destroy)(AbiVariant*);
 void(*deallocate)(void*,std::ptrdiff_t,std::ptrdiff_t);
 const void*(*cast)(const void*,const void*);const void*meta_item,*meta_itemview;
 AbiMetaCall itemview;
 void*(*content)(const void*);double(*content_height)(const void*);double(*bottom_margin)(const void*);void(*set_bottom_margin)(void*,double);
};
struct MenuTarget {void*page=nullptr;void*list=nullptr;void*content=nullptr;double rowHeight=88;};
// Traverses actual visual children, bounded and unique. Does not mutate Qt.
int find_exposure_menu(const MenuProfile&,void*root,MenuTarget&,void*ownedList=nullptr,double ownedMargin=0);
bool menu_contains(const MenuProfile&,void*root,void*object);
}
