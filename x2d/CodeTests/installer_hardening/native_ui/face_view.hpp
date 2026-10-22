#pragma once
#include "face_model.hpp"
namespace x2dui {
enum class FaceDrawKind {Rectangle,RoundedRectangle,Text};
struct FaceDraw {FaceDrawKind kind=FaceDrawKind::Rectangle;double x=0,y=0,width=0,height=0,radius=0,border=0;uint32_t fill=0,stroke=0;const char16_t*text=nullptr;size_t length=0;int pixels=0;};
struct FaceFrame {FaceDraw commands[560]{};unsigned count=0;double width=0,height=0;bool valid=false;};
/* 固定容量绘制列表；失败时整个frame不可使用，不渲染部分错误状态。 */
bool face_frame(const FacePointModel&,const EyeModel&main,const EyeModel&fallback,FaceFrame&)noexcept;
struct FacePainterApi {void*context=nullptr;bool(*begin)(void*,double width,double height)=nullptr;bool(*draw)(void*,const FaceDraw&)=nullptr;void(*end)(void*)=nullptr;};
bool face_paint(const FaceFrame&,const FacePainterApi&)noexcept;
}
