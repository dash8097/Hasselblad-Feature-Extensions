#ifndef HBL_MECHANICAL_DIRECT_DISPATCH_H
#define HBL_MECHANICAL_DIRECT_DISPATCH_H

/* 专用线程拥有截止点等待和一次 sendto；不调用 Qt，不回调业务对象。
 * 接口仅接收上层已验证的固定请求；关闭、取消与提交用同一把锁串行化。
 * 取消先取得锁时保证没有提交；提交先取得锁时不会重复或声称已撤回。
 */
#ifdef HBL_DIRECT_DISPATCH_TEST_PLATFORM
#include HBL_DIRECT_DISPATCH_TEST_PLATFORM
#else
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#endif

class MechanicalDirectDispatch {
public:
    enum Result { Empty=0, Submitted=1, Cancelled=2, Failed=3 };
    MechanicalDirectDispatch() {
        if (pthread_mutex_init(&mutex,nullptr)) return;
        mutexReady=true;
        wake=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);
        done=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);
        timer=timerfd_create(CLOCK_MONOTONIC,TFD_CLOEXEC|TFD_NONBLOCK);
        if (wake<0 || done<0 || timer<0) return;
        pthread_attr_t attr;
        if (pthread_attr_init(&attr)) return;
        sched_param priority={}; priority.sched_priority=1;
        bool configured=pthread_attr_setinheritsched(&attr,PTHREAD_EXPLICIT_SCHED)==0 &&
                        pthread_attr_setschedpolicy(&attr,SCHED_FIFO)==0 &&
                        pthread_attr_setschedparam(&attr,&priority)==0;
        if (configured) threadReady=pthread_create(&thread,&attr,entry,this)==0;
        pthread_attr_destroy(&attr);
    }
    ~MechanicalDirectDispatch() {
        if (threadReady) {
            pthread_mutex_lock(&mutex);
            stopping=true; cancelLocked();
            pthread_mutex_unlock(&mutex);
            signal(wake);
            pthread_join(thread,nullptr);
        }
        if (timer>=0) close(timer);
        if (done>=0) close(done);
        if (wake>=0) close(wake);
        if (mutexReady) pthread_mutex_destroy(&mutex);
    }
    MechanicalDirectDispatch(const MechanicalDirectDispatch&)=delete;
    MechanicalDirectDispatch &operator=(const MechanicalDirectDispatch&)=delete;
    bool valid() const { return threadReady; }
    int completionFd() const { return done; }
    /* 不持有调用方的 fd：每个已接受任务保存一份 dup，取消后及时关闭。 */
    bool schedule(int fd,const void *packet,size_t size,const sockaddr *peer,
                  socklen_t peerSize,uint64_t absoluteUs) {
        if (!threadReady || !packet || !peer || !size || size>sizeof(bytes) ||
            peerSize>sizeof(address) || peerSize<sizeof(sa_family_t) ||
            !absoluteUs || absoluteUs/1000000u>INT32_MAX) return false;
        const uint64_t now=nowUs();
        if (!now || (absoluteUs>now && absoluteUs-now>5000000u) ||
            (now>absoluteUs && now-absoluteUs>250000u)) return false;
        pthread_mutex_lock(&mutex);
        if (stopping || broken || pending || result!=Empty) {
            pthread_mutex_unlock(&mutex); return false;
        }
        const int duplicate=fcntl(fd,F_DUPFD_CLOEXEC,0);
        if (duplicate<0) { pthread_mutex_unlock(&mutex); return false; }
        itimerspec specification={};
        specification.it_value.tv_sec=time_t(absoluteUs/1000000u);
        specification.it_value.tv_nsec=long(absoluteUs%1000000u)*1000;
        if (timerfd_settime(timer,TFD_TIMER_ABSTIME,&specification,nullptr)) {
            close(duplicate); pthread_mutex_unlock(&mutex); return false;
        }
        memcpy(bytes,packet,size); memcpy(&address,peer,peerSize);
        packetSize=size; addressSize=peerSize; target=absoluteUs;
        ownedFd=duplicate; pending=true;
        pthread_mutex_unlock(&mutex);
        signal(wake);
        return true;
    }
    /* 完成结果只消费一次。调用方用 completionFd 的可读通知收取。 */
    Result take() {
        if (!mutexReady) return Empty;
        pthread_mutex_lock(&mutex);
        drain(done);
        const Result answer=result; result=Empty;
        pthread_mutex_unlock(&mutex);
        return answer;
    }
    /* 若已提交，返回 Submitted/Failed 并清空通知；绝不把提交伪称取消。 */
    Result cancel() {
        if (!mutexReady) return Empty;
        pthread_mutex_lock(&mutex);
        const Result answer=pending ? cancelLocked() : result;
        result=Empty; drain(done);
        pthread_mutex_unlock(&mutex);
        signal(wake);
        return answer;
    }
private:
    pthread_t thread={};
    pthread_mutex_t mutex={};
    bool mutexReady=false,threadReady=false,stopping=false,broken=false,pending=false;
    int wake=-1,done=-1,timer=-1,ownedFd=-1;
    uint8_t bytes[256]={}; sockaddr_storage address={};
    size_t packetSize=0; socklen_t addressSize=0;
    uint64_t target=0;
    Result result=Empty;
    static uint64_t nowUs() {
        timespec t={};
        if (clock_gettime(CLOCK_MONOTONIC,&t) || t.tv_sec<0 || t.tv_nsec<0) return 0;
        return uint64_t(t.tv_sec)*1000000u+uint64_t(t.tv_nsec)/1000u;
    }
    static void signal(int fd) {
        if (fd<0) return;
        const uint64_t one=1;
        ssize_t count;
        do { count=write(fd,&one,sizeof(one)); } while(count<0 && errno==EINTR);
        /* EAGAIN 表示同一 eventfd 已有未消费通知，无需第二次唤醒。 */
    }
    static void drain(int fd) {
        if (fd<0) return;
        uint64_t count;
        while(read(fd,&count,sizeof(count))==ssize_t(sizeof(count))) {}
    }
    Result cancelLocked() {
        if (!pending) return Empty;
        itimerspec empty={};
        if (timerfd_settime(timer,0,&empty,nullptr)) broken=true;
        drain(timer);
        close(ownedFd); ownedFd=-1; pending=false;
        return Cancelled;
    }
    static void *entry(void *self) {
        static_cast<MechanicalDirectDispatch *>(self)->run(); return nullptr;
    }
    void run() {
        for (;;) {
            pollfd wait[2]={{wake,POLLIN,0},{timer,POLLIN,0}};
            int available;
            do { available=poll(wait,2,-1); } while(available<0 && errno==EINTR);
            pthread_mutex_lock(&mutex);
            if (stopping) { pthread_mutex_unlock(&mutex); return; }
            if (available<0 || (wait[0].revents&(POLLERR|POLLHUP|POLLNVAL)) ||
                (wait[1].revents&(POLLERR|POLLHUP|POLLNVAL))) {
                broken=true;
                if (pending) { cancelLocked(); result=Failed; signal(done); }
                pthread_mutex_unlock(&mutex); return;
            }
            if (wait[0].revents&POLLIN) drain(wake);
            if (wait[1].revents&POLLIN) drain(timer);
            if (pending) {
                const uint64_t now=nowUs();
                if (!now || (now>=target && now-target>250000u)) {
                    cancelLocked(); result=Failed; signal(done);
                } else if (now>=target) {
                    /* 一次任务只调用一次发送；失败（包括 EINTR）也不重发。 */
                    const ssize_t submitted=sendto(ownedFd,bytes,packetSize,MSG_DONTWAIT|MSG_NOSIGNAL,
                                                  reinterpret_cast<const sockaddr *>(&address),addressSize);
                    result=submitted==ssize_t(packetSize) ? Submitted : Failed;
                    close(ownedFd); ownedFd=-1; pending=false;
                    signal(done);
                }
            }
            pthread_mutex_unlock(&mutex);
        }
    }
};
#endif
