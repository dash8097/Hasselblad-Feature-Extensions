#include "../native/latest_cache.h"
#include <cassert>
#include <memory>
#include <thread>
#include <vector>
#ifdef NDEBUG
#error Assertions must be enabled in this test executable
#endif
using Handle=std::shared_ptr<const std::vector<unsigned char>>;
using Cache=X1DMemory::LatestCache<Handle>;
static Handle bytes() { return std::make_shared<const std::vector<unsigned char>>(16,0xab); }
int main() {
    Cache c(32);Handle a=bytes();std::weak_ptr<const std::vector<unsigned char>> wa=a;
    auto e1=c.begin();assert(c.publish(e1,a,16,true));a.reset();auto first=c.acquire(e1);
    assert(first && first.current() && first.data()->at(0)==0xab);
    auto e2=c.begin();assert(!first.current() && !c.acquire(e1));
    auto b=bytes();assert(c.publish(e2,b,16,true));b.reset();auto second=c.acquire(e2);
    assert(second && c.retained()==32 && !wa.expired());
    auto e3=c.begin();auto third=bytes();assert(!c.publish(e3,third,16,true));
    assert(!c.acquire(e2) && !second.current());first.reset();assert(wa.expired());
    assert(c.publish(e3,third,16,true));assert(!c.publish(e2,third,16,true));second.reset();
    assert(c.retained()==16);c.begin();c.collect();assert(c.retained()==0);
    auto e=c.begin();assert(!c.publish(e,third,16,false));assert(!c.publish(e,third,33,true));
    assert(!c.publish(e,third,0,true));assert(!c.publish(0,third,16,true));
    assert(c.publish(e,third,16,true));assert(!c.publish(e,third,16,true));
    auto held=c.acquire(e);auto moved=std::move(held);assert(!held && moved);
    c.begin();moved.reset();assert(c.retained()==0);
    // 并发读取和连续替换；原始 Handle 不在 Lease 外逃逸。
    Cache concurrent(32);std::atomic<uint32_t> latest{0};std::atomic<bool> done{false};
    std::thread reader([&]{ while(!done.load()) { auto lease=concurrent.acquire(latest.load());
        if(lease) assert(lease.data()->at(0)==0xab); } });
    for(unsigned i=0;i<10000;++i) { auto n=concurrent.begin();latest.store(n);concurrent.publish(n,third,16,true); }
    done=true;reader.join();concurrent.begin();concurrent.collect();assert(concurrent.retained()==0);
}
