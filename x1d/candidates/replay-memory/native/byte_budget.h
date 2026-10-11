#ifndef X1D_BYTE_BUDGET_H
#define X1D_BYTE_BUDGET_H
#include <cstdint>
#include <mutex>
namespace X1DMemory {
// 仅覆盖调用方显式预留的内存；原系统、Qt/codec 隐式分配、驱动峰值还需独立实测。
class ByteBudget {
public:
    enum Group { JpegCache=0, InFlight=1, Display=2, Codec=3 };
    class Reservation {
        ByteBudget *owner=nullptr;Group group=JpegCache;uint64_t bytes=0;
        friend class ByteBudget;
        Reservation(ByteBudget *o,Group g,uint64_t n):owner(o),group(g),bytes(n) {}
    public:
        Reservation()=default;Reservation(const Reservation &)=delete;Reservation &operator=(const Reservation &)=delete;
        Reservation(Reservation &&b) noexcept:owner(b.owner),group(b.group),bytes(b.bytes) { b.owner=nullptr; }
        Reservation &operator=(Reservation &&b) noexcept {
            if(this!=&b) { reset();owner=b.owner;group=b.group;bytes=b.bytes;b.owner=nullptr; }return *this;
        }
        ~Reservation() { reset(); }
        explicit operator bool() const { return owner!=nullptr; }
        void reset() {
            if(!owner)return;auto *o=owner;owner=nullptr;
            std::lock_guard<std::mutex> lock(o->mutex);o->used[group]-=bytes;o->total-=bytes;
        }
        // 原块成为最终缓存时只转移计费，不凭空减少全局存活量；有复制时仍需另预留。
        bool transfer(Group target) {
            if(!owner)return false;std::unique_lock<std::mutex> lock(owner->mutex,std::try_to_lock);
            if(!lock.owns_lock() || target<JpegCache || target>Codec)return false;
            if(target==group)return true;
            if(bytes>owner->caps[target]-owner->used[target])return false;
            owner->used[group]-=bytes;owner->used[target]+=bytes;group=target;return true;
        }
    };
    ByteBudget(uint64_t maximum,uint64_t jpeg,uint64_t input,uint64_t display,uint64_t codec)
        :limit(maximum),caps{jpeg,input,display,codec} {}
    ByteBudget(const ByteBudget &)=delete;ByteBudget &operator=(const ByteBudget &)=delete;
    Reservation reserve(Group g,uint64_t n) {
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || g<JpegCache || g>Codec || !n || n>limit-total || n>caps[g]-used[g])return {};
        total+=n;used[g]+=n;return Reservation(this,g,n);
    }
    uint64_t retained() { std::lock_guard<std::mutex> lock(mutex);return total; }
private:
    std::mutex mutex;const uint64_t limit;const uint64_t caps[4];uint64_t used[4]={0,0,0,0},total=0;
};
}
#endif
