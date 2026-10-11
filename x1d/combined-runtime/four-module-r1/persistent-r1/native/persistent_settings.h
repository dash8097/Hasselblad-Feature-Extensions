#ifndef HBL_PERSISTENT_SETTINGS_H
#define HBL_PERSISTENT_SETTINGS_H
#include <stdint.h>
#include <stddef.h>
#include "mechanical_calibration.h"

/* 固定小记录：不保存照片、相机身份或会话状态。显式编码，不落盘 C++ ABI。 */
struct HblPersistentSettings {
    enum { Master=1, Power=2, Sync=4, Thirds=8, Words=60, Bytes=(Words+3)*4 };
    uint32_t flags,channel,id,visible;
    uint32_t calibration[5],active[16],tenths[16],lamps[16];
    // 预留三个已强制为零的字，后续格式变化必须升级版本。
    uint32_t reserved[3];
    static HblPersistentSettings defaults() {
        HblPersistentSettings s={};s.flags=Power|Sync|Thirds;s.channel=5;s.id=5;s.visible=31;
        hbl_calibration_defaults(s.calibration);
        for(unsigned i=0;i<16;++i)s.tenths[i]=40;
        return s;
    }
    bool valid() const {
        if(flags>15 || !channel || channel>32 || id>99 || visible>65535 ||
           !hbl_calibration_valid(calibration))return false;
        for(unsigned i=0;i<16;++i)if(active[i]>1 || tenths[i]>80 || lamps[i]>1)return false;
        return !reserved[0] && !reserved[1] && !reserved[2];
    }
    static void put(uint8_t *p,uint32_t n) {for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(8*i));}
    static uint32_t get(const uint8_t *p) {uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(p[i])<<(8*i);return n;}
    static uint32_t crc(const uint8_t *p,size_t n) {
        uint32_t c=~uint32_t(0);
        for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}
        return ~c;
    }
    bool encode(uint8_t *p,size_t n) const {
        if(!p || n!=Bytes || !valid())return false;
        put(p,0x31534248);put(p+4,1);unsigned k=2;
        put(p+4*k++,flags);put(p+4*k++,channel);put(p+4*k++,id);put(p+4*k++,visible);
        for(unsigned i=0;i<5;++i)put(p+4*k++,calibration[i]);
        for(unsigned i=0;i<16;++i)put(p+4*k++,active[i]);
        for(unsigned i=0;i<16;++i)put(p+4*k++,tenths[i]);
        for(unsigned i=0;i<16;++i)put(p+4*k++,lamps[i]);
        for(unsigned i=0;i<3;++i)put(p+4*k++,reserved[i]);
        if(k!=Words+2)return false;
        put(p+4*k,crc(p,4*k));return true;
    }
    static bool decode(const uint8_t *p,size_t n,HblPersistentSettings *out) {
        if(!p || !out || n!=Bytes || get(p)!=0x31534248 || get(p+4)!=1 ||
           get(p+Bytes-4)!=crc(p,Bytes-4))return false;
        HblPersistentSettings s={};unsigned k=2;
        s.flags=get(p+4*k++);s.channel=get(p+4*k++);s.id=get(p+4*k++);s.visible=get(p+4*k++);
        for(unsigned i=0;i<5;++i)s.calibration[i]=get(p+4*k++);
        for(unsigned i=0;i<16;++i)s.active[i]=get(p+4*k++);
        for(unsigned i=0;i<16;++i)s.tenths[i]=get(p+4*k++);
        for(unsigned i=0;i<16;++i)s.lamps[i]=get(p+4*k++);
        for(unsigned i=0;i<3;++i)s.reserved[i]=get(p+4*k++);
        if(!s.valid() || k!=Words+2)return false;
        *out=s;return true;
    }
};
#endif
