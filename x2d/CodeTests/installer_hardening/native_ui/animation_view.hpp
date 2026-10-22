#pragma once
#include "shutter_animation_core.hpp"
#include <cstddef>
#include <cstdint>
namespace x2dui {
struct AnimationRect {double x=0,y=0,width=0,height=0;};
struct AnimationTextMetrics {double width=0,height=0,ascent=0;};
enum class AnimationPathVerb:uint32_t {Move,Line,Quadratic,Close};
struct AnimationPathPart {AnimationPathVerb verb;AnimationPoint end{},control{};};
// ABI 边界仅指针/整数/double/POD，不把 Qt 或 C++运行库对象跨边界传递。
// ctx 是一个活动 QPainter 的本地适配上下文；所有回调在同一 GUI 绘制线程。
struct AnimationPainterApi {
 void*ctx=nullptr;
 bool(*save)(void*)=nullptr;
 void(*restore)(void*)=nullptr;
 bool(*opacity)(void*,double)=nullptr;
 bool(*translate)(void*,double,double)=nullptr;
 bool(*scale)(void*,double,double)=nullptr;
 bool(*rotate)(void*,double)=nullptr;
 bool(*clip)(void*,AnimationRect)=nullptr;
 bool(*fill_rect)(void*,AnimationRect,uint32_t rgba)=nullptr;
 bool(*text_metrics)(void*,const char16_t*,size_t,int pixels,AnimationTextMetrics*)=nullptr;
 bool(*text)(void*,const char16_t*,size_t,int pixels,AnimationPoint baseline,uint32_t rgba)=nullptr;
 bool(*path)(void*,const AnimationPathPart*,size_t,uint32_t fillRgba,uint32_t strokeRgba,double width)=nullptr;
};
struct AnimationPaintResult {bool success=false;unsigned leaves=0,texts=0;};
// 黑底铺满宿主；800x600设计等比居中，只有叶片裁到Canvas边界。
// opacity 应只应用一次：宿主item自身opacity保持1，本函数消费frame.overlayOpacity。
// success=false 时绘图状态已恢复，但调用方必须丢弃该半帧纹理，不能覆盖原厂画面。
AnimationPaintResult paint_shutter_animation(const AnimationPainterApi&,const AnimationFrame&,double width,double height);
}
