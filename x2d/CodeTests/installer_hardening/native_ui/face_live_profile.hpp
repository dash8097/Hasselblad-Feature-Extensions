#pragma once
#include "face_factory.hpp"
#include "face_draw_pool.hpp"
namespace x2dui {
struct FaceLiveProfile {
 FaceFactoryProfile factory;
 const void*rectType=nullptr,*pointType=nullptr,*colorType=nullptr,*listMeta=nullptr,*cameraDbusMeta=nullptr;
 // Original VariantMapModel::get returns QVariant, not QQmlListModel's QJSValue.
 AbiVariant(*list_get)(const void*,int)=nullptr;
 const void*mapType=nullptr;
 AbiVariant(*map_value)(const void*,const AbiString&,const AbiVariant&)=nullptr;
 void(*face_target)(void*,int)=nullptr;
 FactoryFieldSpec liveState{},screenStatus{},screensActive{};
 int running=-1,runningCont=-1,roiDefault=-1,roiFace=-1,roiLeft=-1,roiRight=-1;
};
bool face_live_snapshot(const FaceLiveProfile&,const StockProfile&,const MenuProfile&,void*vm,double width,double height,FaceSnapshot&,FaceModeSnapshot&,unsigned*diagnosticStage=nullptr);
// Fixed original VM functions only. No QJS evaluate, arbitrary path/method or camera capture.
bool face_live_apply(const FaceLiveProfile&,const StockProfile&,void*vm,void*indicator,const FaceAction&);
}
