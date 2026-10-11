#include "replay_budget.h"
#include <QtCore/qsemaphore.h>
#include <atomic>

namespace {
QSemaphore fullSlots(1);
std::atomic<uint64_t> releaseEpoch(1);
}
X1D::FullPermit::~FullPermit() {
    fullSlots.release(1);
    releaseEpoch.fetch_add(1);
}
X1D::FullToken X1D::acquireFull() {
    if (!fullSlots.tryAcquire(1)) return FullToken();
    // 仅分配一个带原子引用计数的许可；构造失败立即归还额度。
    FullPermit *p;
    try { p = new FullPermit; }
    catch (...) { fullSlots.release(1); throw; }
    return FullToken(p);
}
uint64_t X1D::fullReleaseEpoch() { return releaseEpoch.load(); }
bool X1D::fullAvailable() { return fullSlots.available()>0; }
