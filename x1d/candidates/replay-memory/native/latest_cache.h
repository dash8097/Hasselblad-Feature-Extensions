#ifndef X1D_LATEST_CACHE_H
#define X1D_LATEST_CACHE_H
#include <atomic>
#include <cstdint>
#include <mutex>
#include <utility>
#include <type_traits>

namespace X1DMemory {
// Handle 是不复制压缩内容、复制/移动 noexcept 的拥有型引用。
// 仅一个生产者调用 begin/publish；读取端只能在 Lease 内使用 Handle。
// 这是缓存生命周期模块，尚未接入生产钩子或跨进程传输。
template<class Handle> class LatestCache {
    static_assert(std::is_nothrow_copy_assignable<Handle>::value,"Handle sharing must not throw");
    static_assert(std::is_nothrow_move_assignable<Handle>::value,"Handle release must not throw");
    struct Slot { Handle bytes{}; uint32_t epoch=0; uint64_t cost=0; unsigned readers=0; };
public:
    class Lease {
        LatestCache *cache=nullptr; unsigned slot=0; uint32_t epoch=0;
        friend class LatestCache;
        Lease(LatestCache *c,unsigned s,uint32_t e):cache(c),slot(s),epoch(e) {}
    public:
        Lease()=default;
        Lease(const Lease &)=delete; Lease &operator=(const Lease &)=delete;
        Lease(Lease &&b) noexcept:cache(b.cache),slot(b.slot),epoch(b.epoch) { b.cache=nullptr; }
        Lease &operator=(Lease &&b) noexcept {
            if(this!=&b) { reset();cache=b.cache;slot=b.slot;epoch=b.epoch;b.cache=nullptr; } return *this;
        }
        ~Lease() { reset(); }
        explicit operator bool() const { return cache!=nullptr; }
        const Handle &data() const { return cache->slots[slot].bytes; }
        bool current() const { return cache && cache->generation.load()==epoch; }
        void reset() {
            if(!cache) return;
            LatestCache *c=cache;cache=nullptr;
            std::lock_guard<std::mutex> lock(c->mutex);
            --c->slots[slot].readers;c->collectLocked();
        }
    };
    explicit LatestCache(uint64_t totalBudget):budget(totalBudget) {}
    LatestCache(const LatestCache &)=delete; LatestCache &operator=(const LatestCache &)=delete;
    // 达到上限后永久关闭本实例，防止 token 重用。对象必须比所有 Lease 长寿。
    uint32_t begin() noexcept {
        uint32_t prior=generation.load();
        if(prior==UINT32_MAX) return 0;
        generation.store(prior+1);
        return prior+1==UINT32_MAX ? 0 : prior+1;
    }
    // 在原写卡调用返回之后才可调用。拒绝缓存不影响原返回值。
    bool publish(uint32_t epoch,const Handle &data,uint64_t allocationCost,bool owned) {
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock.owns_lock()) return false;
        if(!epoch || epoch==UINT32_MAX || epoch!=generation.load() || !owned || !allocationCost || allocationCost>budget) return false;
        for(const auto &s:slots) if(s.epoch==epoch) return false;
        if(allocationCost>budget-charged) return false;
        for(auto &s:slots) if(!s.epoch) {
            s.bytes=data;s.cost=allocationCost;s.epoch=epoch;charged+=allocationCost;return true;
        }
        return false;
    }
    Lease acquire(uint32_t epoch) {
        std::lock_guard<std::mutex> lock(mutex);collectLocked();
        if(!epoch || epoch==UINT32_MAX || epoch!=generation.load()) return {};
        for(unsigned i=0;i<2;++i) if(slots[i].epoch==epoch) {
            ++slots[i].readers;return Lease(this,i,epoch);
        }
        return {};
    }
    // 后台/读取线程收走已失效且无读取者的旧引用；begin 自身不等待锁或执行大块释放。
    void collect() { std::lock_guard<std::mutex> lock(mutex);collectLocked(); }
    uint64_t retained() { std::lock_guard<std::mutex> lock(mutex);return charged; }
private:
    void collectLocked() {
        for(auto &s:slots) if(s.epoch && s.epoch!=generation.load() && !s.readers) {
            charged-=s.cost;s.bytes=Handle{};s.cost=0;s.epoch=0;
        }
    }
    std::atomic<uint32_t> generation{0}; std::mutex mutex; Slot slots[2];
    const uint64_t budget; uint64_t charged=0;
};
}
#endif
