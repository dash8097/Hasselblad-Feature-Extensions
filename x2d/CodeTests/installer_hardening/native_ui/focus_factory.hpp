#pragma once
#include "face_factory.hpp"
#include "focus_model.hpp"
namespace x2dui {
struct FocusFactoryProfile {FaceFactoryProfile common;unsigned languageIndex=20;unsigned languageWidth=2;};
bool focus_factory_snapshot(const FocusFactoryProfile&,const StockProfile&,const MenuProfile&,void*popup,void*model,FocusSnapshot&,char16_t*heading,size_t capacity);
bool focus_factory_select(const FocusFactoryProfile&,const StockProfile&,void*popup,int mode);
}
