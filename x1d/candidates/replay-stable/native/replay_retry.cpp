#include "replay_retry.h"
#include "replay_budget.h"
#include <QtCore/qmutex.h>
#include <QtCore/qelapsedtimer.h>

namespace {
QMutex mutex;
QString current;
bool selected=false;
QString pending;
uint64_t observedEpoch=0;
X1D::RetryCause why=X1D::ResourceBusy;
QElapsedTimer delay;
}
void X1D::selectFullSource(const QString &source) {
    QMutexLocker lock(&mutex);
    selected=true;
    if (current!=source) { current=source; pending.clear(); }
}
bool X1D::currentFullSource(const QString &source) {
    QMutexLocker lock(&mutex);
    // 离线接口或非 QQuickImage 消费者没有 QML 选择记录。
    return !selected || current==source;
}
void X1D::retryFull(const QString &source,RetryCause cause) {
    QMutexLocker lock(&mutex);
    if (selected && current!=source) return;
    pending=source; why=cause; observedEpoch=fullReleaseEpoch(); delay.start();
}
bool X1D::consumeFullRetry(const QString &source) {
    QMutexLocker lock(&mutex);
    if (pending!=source || current!=source) return false;
    // GPU 上传拒绝不循环重新分配；后续用户重新放大仍可启动正常请求。
    if (why==UploadRejected) { pending.clear(); return false; }
    bool ready=why==ResourceBusy ? (fullAvailable() || fullReleaseEpoch()!=observedEpoch) : delay.hasExpired(250);
    if (!ready) return false;
    pending.clear(); return true;
}
