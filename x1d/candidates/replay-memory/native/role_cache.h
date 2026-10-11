#ifndef X1D_ROLE_CACHE_H
#define X1D_ROLE_CACHE_H
#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <utility>
namespace X1DMemory {
// 四个身份槽也包含仍被旧读取者使用的槽。identity 必须由上层证明不可变，不能用文件名替代。
// Handle 共享已经计费的压缩/像素数据；生产者单线程，beginLatest/publish 不等待消费者。
template<class Handle> class RoleCache {
public:
    enum Role { Latest=0, Current=1, Previous=2, Next=3 };
    struct Ticket { Role role=Latest; uint32_t request=0; uint64_t identity=0;
        Ticket()=default; Ticket(Role r,uint32_t n,uint64_t k):role(r),request(n),identity(k) {} };
private:
    static_assert(std::is_nothrow_copy_assignable<Handle>::value,"Handle copy must not throw");
    static_assert(std::is_nothrow_move_assignable<Handle>::value,"Handle move must not throw");
    struct Slot { Handle bytes{};uint64_t identity=0,cost=0;uint32_t capture=0;unsigned readers=0; };
public:
    class Lease {
        RoleCache *owner=nullptr;unsigned index=0;Ticket ticket;
        friend class RoleCache;
        Lease(RoleCache *o,unsigned i,Ticket t):owner(o),index(i),ticket(t) {}
    public:
        Lease()=default;Lease(const Lease &)=delete;Lease &operator=(const Lease &)=delete;
        Lease(Lease &&b) noexcept:owner(b.owner),index(b.index),ticket(b.ticket) { b.owner=nullptr; }
        Lease &operator=(Lease &&b) noexcept {
            if(this!=&b) { reset();owner=b.owner;index=b.index;ticket=b.ticket;b.owner=nullptr; }return *this;
        }
        ~Lease() { reset(); }
        explicit operator bool() const { return owner!=nullptr; }
        const Handle &data() const { return owner->slots[index].bytes; }
        bool current() const {
            if(!owner) return false;std::lock_guard<std::mutex> lock(owner->mutex);return owner->valid(ticket);
        }
        void reset() {
            if(!owner) return;auto *o=owner;owner=nullptr;
            std::lock_guard<std::mutex> lock(o->mutex);--o->slots[index].readers;o->collectLocked();
        }
    };
    explicit RoleCache(uint64_t byteLimit):limit(byteLimit) {}
    RoleCache(const RoleCache &)=delete;RoleCache &operator=(const RoleCache &)=delete;
    Ticket beginLatest(uint64_t identity) noexcept {
        const uint32_t n=capture.load();if(n==UINT32_MAX) return {};
        capture.store(n+1);return n+1==UINT32_MAX ? Ticket{} : Ticket(Latest,n+1,identity);
    }
    // 外层在模型快照中确定前后邻图。本方法不会因为预取完成而改变 Current。
    void setWindow(uint64_t current,uint64_t previous,uint64_t next) {
        std::lock_guard<std::mutex> lock(mutex);
        if(window==UINT32_MAX) return;
        ++window;wanted[Current]=current;wanted[Previous]=previous;wanted[Next]=next;
        if(window==UINT32_MAX) wanted[Current]=wanted[Previous]=wanted[Next]=0;
        collectLocked();
    }
    void exitManual() { setWindow(0,0,0); }
    Ticket request(Role role) {
        std::lock_guard<std::mutex> lock(mutex);
        if(role<Latest || role>Next) return {};
        if(role==Latest) { for(const auto &s:slots) if(s.capture && s.capture==capture.load()) return Ticket(Latest,s.capture,s.identity);return {}; }
        return Ticket(role,window,wanted[role]);
    }
    bool publish(Ticket t,const Handle &data,uint64_t cost,bool owned=true) {
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || !valid(t) || !owned || !cost || cost>limit) return false;
        for(auto &s:slots) if(s.identity==t.identity) {
            if(t.role==Latest) s.capture=t.request;return true; // 同身份复用，不重复保存传入数据。
        }
        if(cost>limit-used) return false;
        for(auto &s:slots) if(!s.identity) {
            s.bytes=data;s.identity=t.identity;s.cost=cost;s.capture=t.role==Latest?t.request:0;used+=cost;return true;
        }
        return false;
    }
    Lease acquire(Ticket t) {
        std::lock_guard<std::mutex> lock(mutex);collectLocked();if(!valid(t)) return {};
        for(unsigned i=0;i<4;++i) if(slots[i].identity==t.identity) { ++slots[i].readers;return Lease(this,i,t); }
        return {};
    }
    void collect() { std::lock_guard<std::mutex> lock(mutex);collectLocked(); }
    uint64_t retained() { std::lock_guard<std::mutex> lock(mutex);return used; }
    unsigned identities() { std::lock_guard<std::mutex> lock(mutex);unsigned n=0;for(const auto &s:slots)n+=s.identity!=0;return n; }
private:
    bool valid(Ticket t) const {
        if(!t.identity || !t.request || t.request==UINT32_MAX) return false;
        if(t.role==Latest) return t.request==capture.load();
        return t.role>=Current && t.role<=Next && t.request==window && t.identity==wanted[t.role];
    }
    bool pinned(const Slot &s) const {
        return (s.capture && s.capture==capture.load()) || s.identity==wanted[Current] || s.identity==wanted[Previous] || s.identity==wanted[Next];
    }
    void collectLocked() {
        for(auto &s:slots) if(s.identity && !s.readers && !pinned(s)) {
            used-=s.cost;s.bytes=Handle{};s.identity=0;s.cost=0;s.capture=0;
        }
    }
    std::atomic<uint32_t> capture{0};std::mutex mutex;Slot slots[4];
    uint64_t wanted[4]={0,0,0,0};uint32_t window=0;const uint64_t limit;uint64_t used=0;
};
}
#endif
