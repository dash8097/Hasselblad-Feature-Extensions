#include "../native/byte_budget.h"
#include <cassert>
#include <utility>
#ifdef NDEBUG
#error Assertions must be enabled
#endif
using Budget=X1DMemory::ByteBudget;
static const uint64_t MiB=1024*1024;
int main() {
    Budget b(96*MiB,64*MiB,32*MiB,16*MiB,8*MiB);
    auto jpeg=b.reserve(Budget::JpegCache,48*MiB),input=b.reserve(Budget::InFlight,12*MiB);
    auto display=b.reserve(Budget::Display,16*MiB),codec=b.reserve(Budget::Codec,8*MiB);
    assert(jpeg && input && display && codec && b.retained()==84*MiB);
    assert(!b.reserve(Budget::InFlight,13*MiB)); // 单组还放得下，全局已不够。
    assert(input.transfer(Budget::JpegCache));assert(b.retained()==84*MiB);
    auto extra=b.reserve(Budget::InFlight,8*MiB);assert(extra);assert(!extra.transfer(Budget::JpegCache));
    extra.reset();input.reset();assert(b.retained()==72*MiB);
    assert(!b.reserve(Budget::Display,8176ULL*6128*4)); // Full RGB32 不在本候选默认额度内。
    auto moved=std::move(jpeg);assert(!jpeg && moved);moved.reset();display.reset();codec.reset();
    assert(b.retained()==0);assert(!b.reserve(Budget::JpegCache,65*MiB));assert(!b.reserve(Budget::Codec,0));
}
