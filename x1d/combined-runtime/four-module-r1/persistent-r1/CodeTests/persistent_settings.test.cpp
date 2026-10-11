#include "../native/persistent_settings.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
    auto s=HblPersistentSettings::defaults();uint8_t bytes[HblPersistentSettings::Bytes];
    assert(s.valid() && s.encode(bytes,sizeof(bytes)));
    HblPersistentSettings r={};assert(HblPersistentSettings::decode(bytes,sizeof(bytes),&r));
    assert(r.flags==14 && r.channel==5 && r.id==5 && r.visible==31 && r.calibration[4]==6900);
    s.flags=15;s.channel=32;s.id=0;s.visible=0;
    for(unsigned i=0;i<16;++i){s.active[i]=i%2;s.tenths[i]=i*5;s.lamps[i]=(i+1)%2;}
    for(unsigned i=0;i<5;++i)s.calibration[i]=(5-i)*1000;
    assert(s.encode(bytes,sizeof(bytes)) && HblPersistentSettings::decode(bytes,sizeof(bytes),&r));
    uint8_t after[sizeof(bytes)];assert(r.encode(after,sizeof(after)) && !std::memcmp(bytes,after,sizeof(bytes)));
    unsigned checks=4;
    for(unsigned i=0;i<sizeof(bytes);++i)for(unsigned bit=0;bit<8;++bit){
        bytes[i]^=1u<<bit;assert(!HblPersistentSettings::decode(bytes,sizeof(bytes),&r));bytes[i]^=1u<<bit;++checks;
    }
    for(size_t n=0;n<sizeof(bytes);++n){assert(!HblPersistentSettings::decode(bytes,n,&r));++checks;}
    // CRC 正确但版本/业务内容错误仍拒绝，输出记录不被部分修改。
    for(unsigned word=1;word<HblPersistentSettings::Words+2;++word){
        std::memcpy(after,bytes,sizeof(bytes));HblPersistentSettings::put(after+word*4,0xffffffff);
        HblPersistentSettings::put(after+sizeof(bytes)-4,HblPersistentSettings::crc(after,sizeof(bytes)-4));
        const auto old=r;assert(!HblPersistentSettings::decode(after,sizeof(after),&r));assert(!std::memcmp(&r,&old,sizeof(r)));++checks;
    }
    s.calibration[0]=5000000;assert(s.valid());s.calibration[0]=5000001;assert(!s.valid());
    s=HblPersistentSettings::defaults();s.visible=65535;assert(s.valid());s.visible=65536;assert(!s.valid());checks+=4;
    std::printf("persistent-settings-checks=%u hardware-requests=0\n",checks);
}
