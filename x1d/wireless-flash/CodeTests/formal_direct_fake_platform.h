#pragma once
#include "mechanical_direct_fake_platform.h"
#include <deque>
struct sockaddr_un { sa_family_t sun_family;char sun_path[108]; };
#define AF_UNIX 1
#define SOCK_DGRAM 2
#define SOCK_CLOEXEC 4
#define SOCK_NONBLOCK 8
namespace formal_direct_test {
namespace base=hbl_direct_test;
static std::map<std::string,std::shared_ptr<base::State>> bindings;
static std::map<base::State *,std::deque<std::vector<uint8_t>>> queues;
static std::mutex gate;
static std::condition_variable changed;
static bool park=false,parked=false;
static unsigned sends=0,duplicateAt=0,corruptAt=0;
static std::function<void(unsigned)> point;
inline int socketFile(int domain,int,int) {
    if(domain!=AF_UNIX) throw std::runtime_error("nonlocal socket");return base::add(base::State::Socket);
}
inline int bindFile(int fd,const sockaddr *address,socklen_t length) {
    std::lock_guard<std::mutex> lock(base::os().mutex);auto state=base::file(fd);
    state->kind=base::State::Event;bindings[std::string(reinterpret_cast<const char *>(address),length)]=state;return 0;
}
inline HblTestSsize sendFile(int fd,const void *bytes,size_t size,int flags,const sockaddr *address,socklen_t length) {
    auto result=base::send(fd,bytes,size,flags,address,length);
    if(result!=HblTestSsize(size)) return result;
    std::lock_guard<std::mutex> lock(base::os().mutex);
    auto found=bindings.find(std::string(reinterpret_cast<const char *>(address),length));
    if(found==bindings.end()) { errno=ENOENT;return -1; }
    ++sends;auto receiver=found->second;
    std::vector<uint8_t> packet(static_cast<const uint8_t *>(bytes),static_cast<const uint8_t *>(bytes)+size);
    if(sends==corruptAt) packet[0]^=1;
    queues[receiver.get()].push_back(packet);
    if(sends==duplicateAt) queues[receiver.get()].push_back(packet);
    receiver->event=queues[receiver.get()].size();base::os().changed.notify_all();return result;
}
inline HblTestSsize receiveFile(int fd,void *out,size_t capacity,int) {
    std::lock_guard<std::mutex> lock(base::os().mutex);auto state=base::file(fd);
    auto &queue=queues[state.get()];if(queue.empty()) { errno=EAGAIN;return -1; }
    auto packet=queue.front();queue.pop_front();state->event=queue.size();
    const auto size=std::min(capacity,packet.size());memcpy(out,packet.data(),size);return HblTestSsize(size);
}
inline int closeFile(int fd) {
    { std::lock_guard<std::mutex> lock(base::os().mutex);auto state=base::file(fd);
      for(auto it=bindings.begin();it!=bindings.end();) {
          if(it->second==state) { queues.erase(state.get());it=bindings.erase(it); } else ++it;
      }
    }
    return base::closeFile(fd);
}
inline int pollFiles(base::PollFd *items,unsigned count,int timeout) {
    if(count==2) {
        std::unique_lock<std::mutex> lock(gate);
        if(park) { parked=true;changed.notify_all();changed.wait(lock,[] { return !park; });parked=false; }
    }
    return base::pollFiles(items,count,timeout);
}
inline void parkDriver() {
    { std::lock_guard<std::mutex> lock(gate);park=true; }
    int wake=-1;
    { std::lock_guard<std::mutex> lock(base::os().mutex);
      for(const auto &entry:base::os().files) if(entry.second->kind==base::State::Event) { wake=entry.first;break; }
    }
    const uint64_t one=1;base::writeFile(wake,&one,sizeof(one));
    std::unique_lock<std::mutex> lock(gate);
    if(!changed.wait_for(lock,std::chrono::seconds(2),[] { return parked; })) throw std::runtime_error("driver did not park");
}
inline void resumeDriver() { std::lock_guard<std::mutex> lock(gate);park=false;changed.notify_all(); }
inline unsigned submittedCount() { std::lock_guard<std::mutex> lock(base::os().mutex);return sends; }
inline void injectEarlyPacket() {
    std::lock_guard<std::mutex> lock(base::os().mutex);
    auto receiver=bindings.begin()->second;
    queues[receiver.get()].push_back({0x44,0x49,0x52,0x31});
    receiver->event=queues[receiver.get()].size();base::os().changed.notify_all();
}
inline void waitForNextPacket() {
    std::unique_lock<std::mutex> lock(base::os().mutex);
    if(!base::os().changed.wait_for(lock,std::chrono::seconds(2),[] {
        for(const auto &entry:queues) if(!entry.second.empty()) return true;return false;
    })) throw std::runtime_error("thread did not deliver");
}
inline void checkpoint(unsigned stage) { if(point) point(stage); }
}
#undef sendto
#undef close
#undef poll
#define sendto formal_direct_test::sendFile
#define close formal_direct_test::closeFile
#define poll formal_direct_test::pollFiles
#define socket formal_direct_test::socketFile
#define bind formal_direct_test::bindFile
#define recv formal_direct_test::receiveFile
#define getpid() 1234
#define HBL_FORMAL_DIRECT_CHECK_TEST_PLATFORM 1
#define HBL_FORMAL_DIRECT_CHECK_POINT(p) formal_direct_test::checkpoint(p)
