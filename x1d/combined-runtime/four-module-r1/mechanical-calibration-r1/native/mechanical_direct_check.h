#ifndef HBL_MECHANICAL_DIRECT_CHECK_H
#define HBL_MECHANICAL_DIRECT_CHECK_H
#include "mechanical_direct_dispatch.h"
#include <sys/un.h>
#include <stddef.h>
#include <cstdio>

/* 机内功能检查只向本进程的 AF_UNIX 抽象套接字发固定四字节测试数据。
 * 不打开无线接口、不构造 NetlinkRadio/Worker、不连接相机控制服务。
 */
static inline int mechanical_direct_check() {
    MechanicalDirectDispatch direct;
    if (!direct.valid()) return 80;
    struct Descriptors {
        int source=-1,receiver=-1;
        ~Descriptors() { if(source>=0) close(source); if(receiver>=0) close(receiver); }
    } sockets;
    sockets.source=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    sockets.receiver=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if (sockets.source<0 || sockets.receiver<0) return 81;
    sockaddr_un address={}; address.sun_family=AF_UNIX;
    const int count=std::snprintf(address.sun_path+1,sizeof(address.sun_path)-1,
                                  "hbl-direct-check-%ld",long(getpid()));
    if (count<=0 || size_t(count)>=sizeof(address.sun_path)-1) return 82;
    const socklen_t length=socklen_t(offsetof(sockaddr_un,sun_path)+1+count);
    if (bind(sockets.receiver,reinterpret_cast<sockaddr *>(&address),length)) return 83;
    const uint8_t bytes[4]={0x44,0x49,0x52,0x31};
    auto now=[]() -> uint64_t {
        timespec t={};
        if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
        return uint64_t(t.tv_sec)*1000000u+uint64_t(t.tv_nsec)/1000u;
    };
    auto schedule=[&](uint64_t target) {
        return direct.schedule(sockets.source,bytes,sizeof(bytes),
                               reinterpret_cast<const sockaddr *>(&address),length,target);
    };
    auto received=[&]() {
        uint8_t result[8]={};
        const ssize_t size=recv(sockets.receiver,result,sizeof(result),MSG_DONTWAIT);
        return size==ssize_t(sizeof(bytes)) && memcmp(bytes,result,sizeof(bytes))==0;
    };
    auto completed=[&]() {
        pollfd ready={direct.completionFd(),POLLIN,0};
        if (poll(&ready,1,500)!=1 || !(ready.revents&POLLIN)) return false;
        return direct.take()==MechanicalDirectDispatch::Submitted &&
               direct.take()==MechanicalDirectDispatch::Empty && received();
    };
    const uint64_t begin=now();
    if (!begin || schedule(0) || schedule(begin+6000000u) ||
        direct.schedule(sockets.source,bytes,257,reinterpret_cast<const sockaddr *>(&address),length,begin+20000)) return 84;
    const uint64_t first=now()+20000u;
    if (!schedule(first) || schedule(first+1000u)) return 85;
    if (!completed() || now()<first) return 86;
    if (!schedule(now()+50000u) || direct.cancel()!=MechanicalDirectDispatch::Cancelled ||
        direct.take()!=MechanicalDirectDispatch::Empty) return 87;
    pollfd cancelled={sockets.receiver,POLLIN,0};
    if (poll(&cancelled,1,60)!=0) return 88;
    if (!schedule(now()+20000u)) return 89;
    close(sockets.source); sockets.source=-1;
    if (!completed()) return 90;
    sockets.source=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if (sockets.source<0 || !schedule(now()+2000u) ||
        direct.cancel()!=MechanicalDirectDispatch::Cancelled) return 91;
    const uint64_t replacement=now()+20000u;
    if (!schedule(replacement)) return 92;
    pollfd noEarly={sockets.receiver,POLLIN,0};
    if (poll(&noEarly,1,5)!=0 || !completed() || now()<replacement) return 93;
    if (direct.cancel()!=MechanicalDirectDispatch::Empty) return 94;
    std::puts("direct-dispatch-check=8 policy=fifo priority=1 camera-requests=0");
    return 0;
}
#endif
