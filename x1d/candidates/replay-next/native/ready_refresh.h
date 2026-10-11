#ifndef X1D_READY_REFRESH_H
#define X1D_READY_REFRESH_H
#include <QtCore/qurl.h>
class QQmlEngine;
namespace X1D {
/* 仅由固定 1.25.0 Qt load 调用点且运行库哈希匹配的 hook 调用。 */
void trackPixmap(void *pixmap, QQmlEngine *engine, const QUrl &url, quintptr caller);
}
#endif
