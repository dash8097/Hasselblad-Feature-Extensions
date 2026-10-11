#ifndef HBL_MECHANICAL_DEADLINE_H
#define HBL_MECHANICAL_DEADLINE_H
#include <QtCore/qsocketnotifier.h>
#include <functional>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

static inline uint64_t mechanical_monotonic_us() {
    timespec t={};
    if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return uint64_t(t.tv_sec)*1000000u+uint64_t(t.tv_nsec)/1000u;
}
// 绝对 CLOCK_MONOTONIC 截止点；10 us 是设置粒度，Linux 唤醒误差仍需实测。
class MechanicalDeadline {
public:
    explicit MechanicalDeadline(QObject *owner) {
        fd=timerfd_create(CLOCK_MONOTONIC,TFD_CLOEXEC|TFD_NONBLOCK);
        if (fd<0) return;
        notifier=new QSocketNotifier(fd,QSocketNotifier::Read,owner);
        QObject::connect(notifier,&QSocketNotifier::activated,owner,[this](int) {
            uint64_t ticks=0;
            const ssize_t n=read(fd,&ticks,sizeof(ticks));
            if (n==ssize_t(sizeof(ticks)) && ticks) { if (expired) expired(); }
            else if (n<0 && (errno==EAGAIN || errno==EINTR)) return;
            else if (failed) failed();
        });
    }
    ~MechanicalDeadline() { delete notifier; if(fd>=0) close(fd); }
    bool valid() const { return fd>=0; }
    bool start(uint64_t absoluteUs) {
        if (fd<0 || !absoluteUs || absoluteUs/1000000u>INT32_MAX) return false;
        itimerspec t={};
        t.it_value.tv_sec=time_t(absoluteUs/1000000u);
        t.it_value.tv_nsec=long(absoluteUs%1000000u)*1000;
        return timerfd_settime(fd,TFD_TIMER_ABSTIME,&t,nullptr)==0;
    }
    void stop() {
        if (fd<0) return;
        itimerspec t={};
        timerfd_settime(fd,0,&t,nullptr);
        uint64_t ticks; while(read(fd,&ticks,sizeof(ticks))==ssize_t(sizeof(ticks))) {}
    }
    std::function<void()> expired,failed;
private:
    int fd=-1;
    QSocketNotifier *notifier=nullptr;
};
#endif
