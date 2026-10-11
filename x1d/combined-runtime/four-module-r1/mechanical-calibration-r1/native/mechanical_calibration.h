#ifndef HBL_MECHANICAL_CALIBRATION_H
#define HBL_MECHANICAL_CALIBRATION_H
#include <stdint.h>

/* 五个名义曝光锚点；单位微秒。步进为输入量化，不是实测精度。 */
enum { HBL_CALIBRATION_POINTS=5, HBL_CALIBRATION_MAX_US=5000000 };
static inline void hbl_calibration_defaults(uint32_t values[5]) {
    const uint32_t defaults[5]={5000,5000,6300,6900,6900};
    for(unsigned i=0;i<5;++i) values[i]=defaults[i];
}
static inline bool hbl_calibration_valid(const uint32_t *values) {
    if(!values) return false;
    for(unsigned i=0;i<5;++i)
        if(values[i]>HBL_CALIBRATION_MAX_US || values[i]%10) return false;
    return true;
}
static inline bool hbl_calibration_delay(uint64_t exposure,const uint32_t *values,unsigned *out) {
    if(!out || !exposure || !hbl_calibration_valid(values)) return false;
    if(exposure>=8000) { *out=values[0];return true; }
    const uint32_t supported[]={6250,5000,4000,3125,2500,2000,1562,1563,1250,1000,800,625,500};
    bool found=false;
    for(unsigned i=0;i<sizeof(supported)/sizeof(supported[0]);++i)
        if(exposure==supported[i]) found=true;
    if(!found) return false;
    const uint32_t anchors[]={8000,4000,2000,1000,500};
    /* 保持原表的名义曝光时间线性插值，兼容 1/640 的微秒取整。
     * 用非负加权和支持递增、递减、零延迟；最后四舍五入到 10 us。 */
    for(unsigned i=0;i<4;++i) {
        if(exposure<=anchors[i] && exposure>=anchors[i+1]) {
            uint64_t span=anchors[i]-anchors[i+1];
            uint64_t sum=uint64_t(values[i])*(exposure-anchors[i+1])+
                         uint64_t(values[i+1])*(anchors[i]-exposure);
            *out=unsigned((sum+span*5)/(span*10)*10);
            return true;
        }
    }
    return false;
}
#endif
