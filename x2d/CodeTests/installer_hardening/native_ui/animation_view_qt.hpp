#pragma once
#include "animation_view.hpp"
class QPainter;
namespace x2dui {
// 离线Qt6.4.1参照适配器，不能直接作为原厂ARM静态Qt符号绑定。
struct QtAnimationPainter {QPainter*painter=nullptr;unsigned depth=0;};
AnimationPainterApi qt_animation_painter(QtAnimationPainter&);
}
