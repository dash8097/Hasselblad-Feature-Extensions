#pragma once
#include "factory_proxy.hpp"
#include "face_popup_view.hpp"
namespace x2dui {
struct FaceFactoryProfile {
 FactoryProxyProfile proxy;FactoryFieldSpec face{},master{},options{};FaceModeConstants modes;
 void(*list_construct)(void*,const void*)=nullptr;void(*list_destroy)(void*)=nullptr;
 std::ptrdiff_t(*list_count)(const void*)=nullptr;void*(*list_at)(const void*,std::ptrdiff_t)=nullptr;
 int(*method_index)(const void*,const char*)=nullptr;int(*metacall)(void*,int,int,void**)=nullptr;
};
bool face_factory_snapshot(const FaceFactoryProfile&,FaceModeSnapshot&);
bool face_factory_apply(const FaceFactoryProfile&,const FaceAction&);
bool face_factory_popup(const FaceFactoryProfile&,const StockProfile&,const MenuProfile&,void*original,FacePopupSnapshot&,char16_t*heading,size_t capacity);
bool face_factory_close_popup(const FaceFactoryProfile&,const StockProfile&,void*original);
}
