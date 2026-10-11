#ifndef X1D_REPLAY_BUDGET_H
#define X1D_REPLAY_BUDGET_H
#include <QtCore/qatomic.h>
#include <stdint.h>

namespace X1D {
class FullToken;
class FullPermit {
public:
    ~FullPermit();
private:
    FullPermit() : references(1) {}
    QAtomicInt references;
    friend class FullToken;
    friend FullToken acquireFull();
};
class FullToken {
public:
    FullToken() : p(nullptr) {}
    FullToken(const FullToken &other) : p(other.p) { if(p) p->references.ref(); }
    ~FullToken() { if(p && !p->references.deref()) delete p; }
    FullToken &operator=(FullToken other) { swap(other); return *this; }
    void swap(FullToken &other) { FullPermit *old=p; p=other.p; other.p=old; }
    void reset() { FullToken empty; swap(empty); }
    explicit operator bool() const { return p!=nullptr; }
private:
    explicit FullToken(FullPermit *value) : p(value) {}
    FullPermit *p;
    friend FullToken acquireFull();
};
FullToken acquireFull();
uint64_t fullReleaseEpoch();
bool fullAvailable();
}
#endif
