#include "../native/role_cache.h"
#include <cassert>
#include <memory>
#include <vector>
#ifdef NDEBUG
#error Assertions must be enabled
#endif
using Handle=std::shared_ptr<const std::vector<unsigned char>>;
using Cache=X1DMemory::RoleCache<Handle>;
static Handle bytes() { return std::make_shared<const std::vector<unsigned char>>(16,0xab); }
int main() {
    Cache c(64);auto data=bytes();auto latest=c.beginLatest(100);assert(c.publish(latest,data,16));
    c.setWindow(90,80,100);auto current=c.request(Cache::Current),previous=c.request(Cache::Previous),next=c.request(Cache::Next);
    assert(c.publish(current,data,16));assert(c.publish(previous,data,16));assert(c.publish(next,data,16));
    assert(c.identities()==3 && c.retained()==48);assert(c.request(Cache::Current).identity==90);
    auto currentLease=c.acquire(current);auto prevLease=c.acquire(previous);assert(currentLease && prevLease);
    // 翻到已经预取的 80，仅重新指定角色；100 仍为拍摄常驻。
    c.setWindow(80,70,90);assert(!currentLease.current() && !prevLease.current());
    auto shown=c.acquire(c.request(Cache::Current));assert(shown && shown.data()==data);
    assert(!c.publish(previous,data,16));assert(c.publish(c.request(Cache::Previous),data,16));
    assert(c.identities()==4 && c.retained()==64 && c.request(Cache::Current).identity==80);
    // 快速翻页时旧读取仍占槽；第五个身份必须拒绝，而不是继续堆帧。
    c.setWindow(60,50,70);assert(c.identities()==4);assert(!c.publish(c.request(Cache::Current),data,16));
    currentLease.reset();prevLease.reset();shown.reset();assert(c.identities()==2);
    assert(c.publish(c.request(Cache::Current),data,16));assert(c.publish(c.request(Cache::Previous),data,16));
    auto late=c.request(Cache::Next);auto pinned=c.acquire(c.request(Cache::Current));
    c.exitManual();assert(c.request(Cache::Current).identity==0 && !c.publish(late,data,16));
    assert(c.identities()==2 && !pinned.current());pinned.reset();assert(c.identities()==1 && c.retained()==16);
    assert(c.acquire(latest));auto old=c.acquire(latest);auto newer=c.beginLatest(101);
    assert(!old.current() && !c.acquire(latest));assert(c.publish(newer,data,16));
    old.reset();assert(c.identities()==1 && c.acquire(newer));
    c.setWindow(101,0,0);assert(c.acquire(c.request(Cache::Current)));assert(c.identities()==1);
    // 新拍只替换 Latest；正在浏览旧拍摄图的 Current 不被自动切换。
    auto newest=c.beginLatest(102);assert(c.publish(newest,data,16));c.collect();
    assert(c.request(Cache::Current).identity==101 && c.identities()==2);
    c.exitManual();assert(c.identities()==1 && c.acquire(newest));
    Cache small(15);auto t=small.beginLatest(1);assert(!small.publish(t,data,16));assert(!small.publish(t,data,1,false));
}
