#pragma once
#include "menu_discovery.hpp"
namespace x2dui {
enum class StockKind {ExposeScreen,ExposeIconText,ExposeProgress,ExposeViewModel,TextValueRow,MenuHeader,Photo,FramedImage,SettingsGeneric,TiledImage,MenuBoolSelector,PopupIconText,Popover,FramedItem,Touchpad};
struct AbiMetaType {const void*iface;};
struct StockProfile {
 void*(*allocate)(std::size_t);unsigned component_size;
 void*(*engine)(const void*);void*(*context)(const void*);
 void(*url_construct)(void*,const AbiString&,int);void(*url_destroy)(void*);
 void(*component_construct)(void*,void*,const void*,int,void*);void(*component_destroy)(void*);int(*status)(const void*);void*(*create)(void*,void*);
 const void*(*cast)(const void*,const void*);const void*meta_item;
 void(*parent)(void*,void*);void(*visual_parent)(void*,void*);void(*object_destroy)(void*);
 void(*variant_bool)(void*,bool);void(*variant_int)(void*,int);void(*variant_double)(void*,double);void(*variant_string)(void*,const AbiString&);void(*variant_destroy)(void*);
 bool(*set)(void*,const char*,const void*);
 int(*property_index)(const void*,const char*);
 bool(*valid_vtable)(void*,const void*,const void*);void*validation_context;
 AbiVariant(*property)(const void*,const char*);bool(*variant_convert)(void*,AbiMetaType);void*(*variant_data)(void*);
 void(*variant_typed)(void*,AbiMetaType,const void*);const void*qobject_pointer_type;
 const void*size_type=nullptr;const void*meta_image=nullptr;
 void(*qml_property_construct)(void*,void*,const AbiString&)=nullptr;bool(*qml_property_write)(const void*,const void*)=nullptr;void(*qml_property_destroy)(void*)=nullptr;
 void(*string_ctor)(void*,const char16_t*,std::ptrdiff_t)=nullptr;void(*string_deallocate)(void*,std::ptrdiff_t,std::ptrdiff_t)=nullptr;
};
// Only original in-process qrc paths, never a disk path or supplied QML text.
void*stock_create(const StockProfile&,StockKind,void*contextObject,void*parentItem);
bool stock_set_text(const StockProfile&,void*,const char*,const char16_t*);
bool stock_set_bool(const StockProfile&,void*,const char*,bool);
bool stock_set_number(const StockProfile&,void*,const char*,double);
bool stock_set_object(const StockProfile&,void*,const char*,void*);
void*stock_get_object(const StockProfile&,void*,const char*);
bool stock_has(const StockProfile&,void*,const char*);
bool stock_get_size(const StockProfile&,void*,const char*,int&,int&);
bool stock_set_size(const StockProfile&,void*,const char*,int,int);
// Explicitly remove a binding only on an owned component, then set its value.
bool stock_override_number(const StockProfile&,void*,const char16_t*,double);
}
