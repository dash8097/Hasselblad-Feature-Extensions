#ifndef X1D_SAFE_TEXTURE_H
#define X1D_SAFE_TEXTURE_H
#include "replay_budget.h"
#include <QtGui/qimage.h>
#include <QtQuick/qsgtexture.h>
namespace X1D {
// 在当前渲染上下文中实际上传，只有成功才交回可见纹理。
QSGTexture *uploadChecked(const QImage &image,const FullToken &permit);
}
#endif
