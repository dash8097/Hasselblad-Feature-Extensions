#pragma once
/* 仅电脑测试：真实宿主线程／锁＋模拟 Linux 文件描述符。
 * 发送只进入进程内计数与字节容器，不调用网络或设备接口。
 */
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <climits>
#include <cerrno>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include <iostream>
typedef long long HblTestSsize;
typedef unsigned socklen_t;
typedef uint16_t sa_family_t;
struct sockaddr { sa_family_t sa_family; char bytes[14]; };
struct sockaddr_storage { sa_family_t ss_family; char bytes[126]; };
#define EFD_CLOEXEC 1
#define EFD_NONBLOCK 2
#define TFD_CLOEXEC 1
#define TFD_NONBLOCK 2
#define TFD_TIMER_ABSTIME 1
#define F_DUPFD_CLOEXEC 1
#define MSG_DONTWAIT 1
#define MSG_NOSIGNAL 2
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#define SCHED_FIFO 1
#define PTHREAD_EXPLICIT_SCHED 1
#define POLLIN 1
#define POLLERR 8
#define POLLHUP 16
#define POLLNVAL 32

namespace hbl_direct_test {
using Clock=std::chrono::steady_clock;
struct PollFd { int fd; short events,revents; };
struct SchedParam { int sched_priority; };
struct Attr { int policy=0,priority=0,inherit=0; };
struct State {
    enum Kind { Event,Timer,Socket } kind;
    uint64_t event=0,target=0;
    unsigned attempts=0,sent=0;
    std::vector<uint8_t> bytes;
    explicit State(Kind k):kind(k){}
};
struct OperatingSystem {
    std::mutex mutex;
    std::condition_variable changed;
    std::map<int,std::shared_ptr<State>> files;
    int nextFd=10;
    bool rejectThread=false,failSend=false,failPoll=false;
    unsigned createdThreads=0,joinedThreads=0;
    std::function<void()> sendBarrier;
};
inline OperatingSystem &os() { static OperatingSystem state; return state; }
inline uint64_t now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count());
}
inline int add(State::Kind kind) {
    std::lock_guard<std::mutex> lock(os().mutex);
    const int fd=os().nextFd++;
    os().files[fd]=std::make_shared<State>(kind); return fd;
}
inline std::shared_ptr<State> file(int fd) {
    auto position=os().files.find(fd);
    return position==os().files.end() ? nullptr : position->second;
}
inline int event(unsigned,int) { return add(State::Event); }
inline int timer(int,int) { return add(State::Timer); }
inline int duplicate(int fd,int,int) {
    std::lock_guard<std::mutex> lock(os().mutex);
    auto original=file(fd);
    if (!original) { errno=EBADF; return -1; }
    const int copy=os().nextFd++; os().files[copy]=original; return copy;
}
inline int closeFile(int fd) {
    std::lock_guard<std::mutex> lock(os().mutex);
    if (!os().files.erase(fd)) { errno=EBADF; return -1; }
    os().changed.notify_all(); return 0;
}
inline HblTestSsize writeFile(int fd,const void *bytes,size_t size) {
    std::lock_guard<std::mutex> lock(os().mutex);
    auto state=file(fd);
    if (!state || state->kind!=State::Event || size!=8) { errno=EBADF; return -1; }
    uint64_t value; std::memcpy(&value,bytes,8); state->event+=value;
    os().changed.notify_all(); return 8;
}
inline HblTestSsize readFile(int fd,void *bytes,size_t size) {
    std::lock_guard<std::mutex> lock(os().mutex);
    auto state=file(fd);
    if (!state || size!=8) { errno=EBADF; return -1; }
    uint64_t value=0;
    if (state->kind==State::Event) { value=state->event; state->event=0; }
    if (state->kind==State::Timer && state->target && now()>=state->target) { value=1; state->target=0; }
    if (!value) { errno=EAGAIN; return -1; }
    std::memcpy(bytes,&value,8); return 8;
}
inline int timerSet(int fd,int,const itimerspec *request,itimerspec *) {
    std::lock_guard<std::mutex> lock(os().mutex);
    auto state=file(fd);
    if (!state || state->kind!=State::Timer) { errno=EBADF; return -1; }
    state->target=uint64_t(request->it_value.tv_sec)*1000000u+uint64_t(request->it_value.tv_nsec)/1000u;
    os().changed.notify_all(); return 0;
}
inline int getClock(int,timespec *result) {
    const uint64_t at=now(); result->tv_sec=time_t(at/1000000u); result->tv_nsec=long(at%1000000u)*1000; return 0;
}
inline int pollFiles(PollFd *items,unsigned count,int timeout) {
    std::unique_lock<std::mutex> lock(os().mutex);
    const uint64_t limit=timeout<0 ? UINT64_MAX : now()+uint64_t(timeout)*1000u;
    for (;;) {
        if (os().failPoll) { os().failPoll=false; errno=EIO; return -1; }
        unsigned ready=0; uint64_t next=limit,at=now();
        for (unsigned i=0;i<count;++i) {
            items[i].revents=0;
            auto state=file(items[i].fd);
            if (!state) items[i].revents=POLLNVAL;
            else if (state->kind==State::Event && state->event) items[i].revents=POLLIN;
            else if (state->kind==State::Timer && state->target) {
                if (state->target<=at) items[i].revents=POLLIN;
                else if (state->target<next) next=state->target;
            }
            if (items[i].revents) ++ready;
        }
        if (ready) return int(ready);
        if (at>=limit) return 0;
        if (next==UINT64_MAX) os().changed.wait(lock);
        else os().changed.wait_until(lock,Clock::time_point(std::chrono::microseconds(next)));
    }
}
inline HblTestSsize send(int fd,const void *bytes,size_t size,int,const sockaddr *,socklen_t) {
    std::shared_ptr<State> state;
    std::function<void()> barrier;
    bool fail;
    {
        std::lock_guard<std::mutex> lock(os().mutex);
        state=file(fd);
        if (!state || state->kind!=State::Socket) { errno=EBADF; return -1; }
        ++state->attempts; barrier=os().sendBarrier; fail=os().failSend; os().failSend=false;
    }
    if (barrier) barrier();
    std::lock_guard<std::mutex> lock(os().mutex);
    if (fail) { errno=EINTR; return -1; }
    ++state->sent;
    state->bytes.assign(static_cast<const uint8_t *>(bytes),static_cast<const uint8_t *>(bytes)+size);
    return HblTestSsize(size);
}
inline int attrInit(Attr *attr) { *attr=Attr{}; return 0; }
inline int attrInherit(Attr *attr,int value) { attr->inherit=value; return 0; }
inline int attrPolicy(Attr *attr,int value) { attr->policy=value; return 0; }
inline int attrPriority(Attr *attr,const SchedParam *value) { attr->priority=value->sched_priority; return 0; }
inline int attrDestroy(Attr *) { return 0; }
inline int create(std::thread **target,const Attr *attr,void *(*entry)(void *),void *context) {
    std::lock_guard<std::mutex> lock(os().mutex);
    if (os().rejectThread) return EPERM;
    if (attr->inherit!=PTHREAD_EXPLICIT_SCHED || attr->policy!=SCHED_FIFO || attr->priority!=1) return EINVAL;
    *target=new std::thread([=]() { entry(context); }); ++os().createdThreads; return 0;
}
inline int join(std::thread *target,void **) {
    target->join(); delete target;
    std::lock_guard<std::mutex> lock(os().mutex); ++os().joinedThreads; return 0;
}
inline int mutexInit(std::mutex *,const void *) { return 0; }
inline int mutexDestroy(std::mutex *) { return 0; }
inline int mutexLock(std::mutex *mutex) { mutex->lock(); return 0; }
inline int mutexUnlock(std::mutex *mutex) { mutex->unlock(); return 0; }
}
#define ssize_t HblTestSsize
#define pthread_t std::thread *
#define pthread_mutex_t std::mutex
#define pthread_attr_t hbl_direct_test::Attr
#define sched_param hbl_direct_test::SchedParam
#define pollfd hbl_direct_test::PollFd
#define pthread_mutex_init hbl_direct_test::mutexInit
#define pthread_mutex_destroy hbl_direct_test::mutexDestroy
#define pthread_mutex_lock hbl_direct_test::mutexLock
#define pthread_mutex_unlock hbl_direct_test::mutexUnlock
#define pthread_attr_init hbl_direct_test::attrInit
#define pthread_attr_setinheritsched hbl_direct_test::attrInherit
#define pthread_attr_setschedpolicy hbl_direct_test::attrPolicy
#define pthread_attr_setschedparam hbl_direct_test::attrPriority
#define pthread_attr_destroy hbl_direct_test::attrDestroy
#define pthread_create hbl_direct_test::create
#define pthread_join hbl_direct_test::join
#define eventfd hbl_direct_test::event
#define timerfd_create hbl_direct_test::timer
#define timerfd_settime hbl_direct_test::timerSet
#define fcntl hbl_direct_test::duplicate
#define close hbl_direct_test::closeFile
#define read hbl_direct_test::readFile
#define write hbl_direct_test::writeFile
#define poll hbl_direct_test::pollFiles
#define clock_gettime hbl_direct_test::getClock
#define sendto hbl_direct_test::send
