#ifndef HBL_RF_BRIDGE_H
#define HBL_RF_BRIDGE_H
#include <stdint.h>
#include <string.h>

enum RfBridgeKind {
    RF_UI_HELLO=1, RF_UI_CONFIG=2, RF_UI_TEST=3, RF_UI_QUERY=4, RF_UI_BYE=5,
    RF_UI_ARM=6,
    RF_BRIDGE_STATUS=32
};
enum RfBridgeValue {
    RV_ON=0, RV_DELAY, RV_POWER, RV_PAGE, RV_BUSY, RV_ARMED, RV_EVENT_ALIVE,
    RV_CLICKS, RV_STARTS, RV_ENDS, RV_READY_EVENTS, RV_QUEUED, RV_SENT, RV_FAILURES,
    RV_CANCELLED, RV_BUSY_SKIPS, RV_SOURCE, RV_PROGRESS, RV_COUNT
};
typedef struct {
    uint32_t magic,version,kind,session,sequence,time_lo,time_hi;
    uint32_t values[RV_COUNT];
    char text[156];
} RfBridgePacket;
static inline void rf_bridge_init(RfBridgePacket *p,unsigned kind,uint32_t session,uint32_t sequence,uint64_t now) {
    memset(p,0,sizeof(*p)); p->magic=UINT32_C(0x32424d48); p->version=2;
    p->kind=kind; p->session=session; p->sequence=sequence;
    p->time_lo=(uint32_t)now; p->time_hi=(uint32_t)(now>>32);
}
static inline uint64_t rf_bridge_time(const RfBridgePacket *p) {
    return ((uint64_t)p->time_hi<<32)|p->time_lo;
}
static inline int rf_bridge_valid(const RfBridgePacket *p,unsigned bytes) {
    if (bytes!=256 || sizeof(*p)!=256 || p->magic!=UINT32_C(0x32424d48) || p->version!=2 || !p->session) return 0;
    return (p->kind>=RF_UI_HELLO && p->kind<=RF_UI_ARM) || p->kind==RF_BRIDGE_STATUS;
}
static inline int rf_bridge_settings_valid(const RfBridgePacket *p) {
    return p->values[RV_ON]<=1 && p->values[RV_DELAY]<=5000000 && p->values[RV_DELAY]%10==0 &&
           p->values[RV_POWER]>=10 && p->values[RV_POWER]<=100 &&
           p->values[RV_PAGE]<=1 && p->values[RV_SOURCE]<7 && p->values[RV_PROGRESS]<=100;
}
#endif
