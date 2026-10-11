#ifndef HBL_RF_CORE_H
#define HBL_RF_CORE_H
#include <stdint.h>
#include <string.h>

/* 独立事件协议；全部多字节字段按 little endian 编码。 */
#define RF_MAGIC UINT32_C(0x31465248)
#define RF_PACKET_BYTES 32
#define RF_BEGIN 1
#define RF_PHASE 2
#define RF_END 3
#define RF_HELLO 4
#define RF_ALL_PHASES 7

typedef struct {
    uint32_t kind, session, shot, phase, detail, sequence;
} RfEvent;
typedef struct {
    uint32_t session, sequence, latest_shot, shot;
    unsigned enabled, source, delay_ms, active, consumed, pending, phase_seen, progress_threshold;
    uint64_t last_ms, deadline_ms;
} RfCore;

static inline uint32_t rf_u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline void rf_put32(unsigned char *p, uint32_t n) {
    p[0]=(unsigned char)n; p[1]=(unsigned char)(n>>8);
    p[2]=(unsigned char)(n>>16); p[3]=(unsigned char)(n>>24);
}
static inline uint32_t rf_checksum(const unsigned char *p) {
    uint32_t h=UINT32_C(2166136261);
    unsigned i;
    for (i=0;i<28;i++) { h^=p[i]; h*=UINT32_C(16777619); }
    return h;
}
static inline int rf_decode(const unsigned char *p, unsigned size, RfEvent *e) {
    if (size!=RF_PACKET_BYTES || rf_u32(p)!=RF_MAGIC ||
        rf_u32(p+28)!=rf_checksum(p)) return 0;
    e->kind=rf_u32(p+4)&255;
    e->phase=(rf_u32(p+4)>>8)&255;
    if ((rf_u32(p+4)>>16)!=1 || e->kind<RF_BEGIN || e->kind>RF_HELLO) return 0;
    e->session=rf_u32(p+8); e->shot=rf_u32(p+12);
    e->detail=rf_u32(p+16); e->sequence=rf_u32(p+20);
    return e->session && e->sequence && rf_u32(p+24)==0 &&
           (e->kind==RF_PHASE ? e->phase<3 : e->phase==0);
}
static inline void rf_encode(unsigned char *p, const RfEvent *e) {
    memset(p,0,RF_PACKET_BYTES); rf_put32(p,RF_MAGIC);
    rf_put32(p+4,UINT32_C(0x10000) | e->kind | (e->phase<<8));
    rf_put32(p+8,e->session); rf_put32(p+12,e->shot);
    rf_put32(p+16,e->detail); rf_put32(p+20,e->sequence);
    rf_put32(p+28,rf_checksum(p));
}
static inline void rf_cancel(RfCore *s) {
    s->active=0; s->pending=0; s->consumed=1;
}
static inline void rf_init(RfCore *s, uint32_t session) {
    memset(s,0,sizeof(*s)); s->session=session;
}
static inline int rf_configure(RfCore *s, unsigned on, unsigned source, unsigned delay) {
    if (on>1 || source>=3 || delay>5000) return 0;
    if (s->enabled!=on || s->source!=source || s->delay_ms!=delay) rf_cancel(s);
    s->enabled=on; s->source=source; s->delay_ms=delay; return 1;
}
static inline int rf_clock(RfCore *s, uint64_t now) {
    if (now<s->last_ms) { rf_cancel(s); return 0; }
    s->last_ms=now; return 1;
}
static inline int rf_progress_configure(RfCore *s,unsigned threshold) {
    if (threshold>100) return 0;
    if (threshold!=s->progress_threshold) rf_cancel(s);
    s->progress_threshold=threshold; return 1;
}
/* 返回 1 仅表示本机接收了有效事件，不表示已测得物理曝光时刻。 */
static inline int rf_event(RfCore *s, const RfEvent *e, uint64_t now) {
    if (!rf_clock(s,now) || e->session!=s->session ||
        e->sequence<=s->sequence) return 0;
    if (s->sequence && e->sequence!=s->sequence+1) rf_cancel(s);
    s->sequence=e->sequence;
    if (e->kind==RF_HELLO) { rf_cancel(s); return 1; }
    if (e->kind==RF_BEGIN) {
        rf_cancel(s);
        if (!e->shot || e->shot<=s->latest_shot) return 0;
        s->latest_shot=e->shot; s->shot=e->shot; s->phase_seen=0;
        s->active=s->enabled && e->detail==1;
        s->consumed=!s->active; return 1;
    }
    if (e->shot!=s->shot || !s->active) return 0;
    if (e->kind==RF_END) { rf_cancel(s); return 1; }
    if (e->kind!=RF_PHASE || e->phase>=3 || (s->phase_seen&(1u<<e->phase))) return 0;
    if (e->phase==1 && e->detail<s->progress_threshold) return 1;
    s->phase_seen|=1u<<e->phase;
    if (e->phase==s->source && !s->consumed) {
        s->consumed=1; s->pending=1;
        s->deadline_ms=now+s->delay_ms;
    }
    return 1;
}
/* 在实际提交无线请求前消费资格。不得因发送失败重试同一曝光。 */
static inline int rf_due(RfCore *s, uint64_t now) {
    if (!rf_clock(s,now) || !s->enabled || !s->active || !s->pending ||
        now<s->deadline_ms) return 0;
    s->pending=0;
    return now-s->deadline_ms<=250;
}
#endif
