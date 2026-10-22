#pragma once
#include "face_view.hpp"
class QPainter;
namespace x2dui {
/* 仅桌面Qt离线参考适配；不是固件QPainter地址绑定。 */
struct FaceQtPainter {QPainter*painter;int depth=0;};
FacePainterApi face_qt_painter(FaceQtPainter&);
}
