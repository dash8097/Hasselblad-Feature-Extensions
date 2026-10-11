#ifndef HBL_FORMAL_DIRECT_CHECK_H
#define HBL_FORMAL_DIRECT_CHECK_H
#include "mechanical_direct_dispatch.h"
#ifndef HBL_FORMAL_DIRECT_CHECK_TEST_PLATFORM
#include <sys/un.h>
#endif
#include <stddef.h>
#include <cstdio>

/* 只检查本进程 AF_UNIX 抽象套接字。取消和发送谁先获得锁，就验证谁的
 * 实际结果；检查主线程迟到不表示发送线程提前。没有无线或相机对象。 */
static inline uint64_t formal_direct_check_now() {
    timespec at={};
    if (clock_gettime(CLOCK_MONOTONIC,&at) || at.tv_sec<0 || at.tv_nsec<0 || at.tv_nsec>=1000000000) return 0;
    return uint64_t(at.tv_sec)*1000000u+uint64_t(at.tv_nsec)/1000u;
}
static inline int formal_direct_check_poll(int fd,unsigned timeoutMs) {
    const uint64_t begin=formal_direct_check_now(),until=begin+uint64_t(timeoutMs)*1000u;
    if (!begin) return -1;
    for (;;) {
        const uint64_t now=formal_direct_check_now();
        if (!now || now<begin) return -1;
        const unsigned remaining=now>=until ? 0u : unsigned((until-now+999u)/1000u);
        pollfd item={fd,POLLIN,0};
        const int result=poll(&item,1,int(remaining));
        if (result<0 && errno==EINTR) { if (formal_direct_check_now()>=until) return 0; continue; }
        if (result<0 || (item.revents&(POLLERR|POLLHUP|POLLNVAL))) return -1;
        return result==1 && (item.revents&POLLIN) ? 1 : result==0 ? 0 : -1;
    }
}
static inline void formal_direct_check_point(unsigned point) {
#ifdef HBL_FORMAL_DIRECT_CHECK_TEST_PLATFORM
    HBL_FORMAL_DIRECT_CHECK_POINT(point);
#else
    (void)point;
#endif
}
static inline int formal_direct_check() {
    MechanicalDirectDispatch direct;
    if (!direct.valid()) return 80;
    struct Descriptors {
        int source=-1,receiver=-1;
        ~Descriptors() { if(source>=0) close(source);if(receiver>=0) close(receiver); }
    } sockets;
    sockets.source=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    sockets.receiver=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if (sockets.source<0 || sockets.receiver<0) return 81;
    sockaddr_un address={};address.sun_family=AF_UNIX;
    const int nameLength=std::snprintf(address.sun_path+1,sizeof(address.sun_path)-1,
                                     "hbl-formal-direct-check-%ld",long(getpid()));
    if (nameLength<=0 || size_t(nameLength)>=sizeof(address.sun_path)-1) return 82;
    const socklen_t length=socklen_t(offsetof(sockaddr_un,sun_path)+1+nameLength);
    if (bind(sockets.receiver,reinterpret_cast<sockaddr *>(&address),length)) return 83;
    const uint8_t bytes[4]={0x44,0x49,0x52,0x31};
    auto schedule=[&](uint64_t target) {
        return direct.schedule(sockets.source,bytes,sizeof(bytes),
                               reinterpret_cast<const sockaddr *>(&address),length,target);
    };
    auto exactlyOne=[&]() {
        uint8_t received[8]={};
        const uint64_t limit=formal_direct_check_now()+500000u;
        ssize_t size;
        do { size=recv(sockets.receiver,received,sizeof(received),MSG_DONTWAIT); }
        while (size<0 && errno==EINTR && formal_direct_check_now()<limit);
        return size==ssize_t(sizeof(bytes)) && !memcmp(received,bytes,sizeof(bytes)) &&
               formal_direct_check_poll(sockets.receiver,10)==0;
    };
    auto submitted=[&](uint64_t target,bool resultAlreadyConsumed) {
        if (!resultAlreadyConsumed && (formal_direct_check_poll(direct.completionFd(),500)!=1 ||
                                       direct.take()!=MechanicalDirectDispatch::Submitted)) return false;
        return direct.take()==MechanicalDirectDispatch::Empty && formal_direct_check_now()>=target && exactlyOne();
    };
    auto cancelledOrSubmitted=[&](uint64_t target,unsigned point) {
        const auto result=direct.cancel();
        formal_direct_check_point(point);
        if (result==MechanicalDirectDispatch::Submitted) return submitted(target,true);
        return result==MechanicalDirectDispatch::Cancelled && direct.take()==MechanicalDirectDispatch::Empty &&
               formal_direct_check_poll(sockets.receiver,60)==0;
    };
    const uint64_t begin=formal_direct_check_now();
    if (!begin || schedule(0) || schedule(begin+6000000u) ||
        direct.schedule(sockets.source,bytes,257,reinterpret_cast<const sockaddr *>(&address),length,begin+20000)) return 84;
    const uint64_t first=formal_direct_check_now()+20000u;
    if (!schedule(first) || schedule(first+1000u)) return 85;
    if (!submitted(first,false)) return 86;

    formal_direct_check_point(1);
    const uint64_t longCancel=formal_direct_check_now()+50000u;
    if (!schedule(longCancel)) return 87;
    formal_direct_check_point(2);
    if (!cancelledOrSubmitted(longCancel,3)) return 88;

    const uint64_t owned=formal_direct_check_now()+20000u;
    if (!schedule(owned)) return 89;
    close(sockets.source);sockets.source=-1;
    if (!submitted(owned,false)) return 90;

    sockets.source=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if (sockets.source<0) return 91;
    formal_direct_check_point(4);
    const uint64_t shortCancel=formal_direct_check_now()+2000u;
    if (!schedule(shortCancel)) return 91;
    formal_direct_check_point(5);
    if (!cancelledOrSubmitted(shortCancel,6)) return 91;

    const uint64_t replacement=formal_direct_check_now()+20000u;
    if (!schedule(replacement)) return 92;
    formal_direct_check_point(7);
    const int readable=formal_direct_check_poll(sockets.receiver,5);
    const uint64_t observedAt=formal_direct_check_now();
    /* poll 返回时调用者可能已迟到：只有仍早于 deadline 的到达才证明早发。
     * 若已到期，保留包给 submitted 一次性检查，不再期待一个新包。 */
    if (readable<0 || !observedAt || (readable==1 && observedAt<replacement) || !submitted(replacement,false)) return 93;
    if (direct.cancel()!=MechanicalDirectDispatch::Empty || formal_direct_check_poll(sockets.receiver,0)!=0) return 94;
    std::puts("formal-direct-check=8 policy=fifo priority=1 camera-requests=0");
    return 0;
}
#endif
