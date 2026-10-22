/* 底层候选试闪：同一生产门控同时用于 AArch64 与离线检查。 */
#ifndef X2D_SENSOR_FLASH_CORE_H
#define X2D_SENSOR_FLASH_CORE_H
#include <stdint.h>
#include <stddef.h>
#define FLASH_WINDOW_NS 60000000000ull
#define FLASH_LATE_NS 250000000ull
enum { F_CAPTURE=1,F_VSYNC,F_TG_ENTRY,F_TG_WRITTEN,F_CH_ENTRY,F_CH_WRITTEN,F_IRQ,F_SOF,F_STATE2 };
typedef struct {
    uint64_t time_ns,caller;
    unsigned type,tid,value,channel,eshutter;
} FlashEvent;
typedef struct {
    unsigned source,delay_us,enabled,sequence,cycle,tid,cycle_valid,consumed,pending;
    unsigned pair_wait,pair_tid,pair_channel,attempts,matched,skipped;
    uint64_t configured_ns,start_ns,last_event_ns,pair_ns,event_ns,deadline_ns,late_ns;
} FlashCore;
static inline void flash_cancel(FlashCore *g) {
    g->enabled=0;g->cycle_valid=0;g->pending=0;g->pair_wait=0;g->consumed=1;
}
/* 心跳只用于显示通信状态，不能改变用户开关或曝光周期。 */
static inline int flash_ui_stale(uint64_t now,uint64_t last_ping) {
    return !now||!last_ping||now<last_ping||now-last_ping>2000000000ull;
}
static inline int flash_configure(FlashCore *g,uint64_t now,unsigned seq,unsigned source,unsigned delay,unsigned on) {
    if(!now||seq<=g->sequence||source<1||source>7||delay>5000000||on>1){flash_cancel(g);return 0;}
    flash_cancel(g);g->sequence=seq;g->source=source;g->delay_us=delay;
    g->configured_ns=now;g->enabled=on;g->last_event_ns=0;return 1;
}
static inline int flash_event(FlashCore *g,const FlashEvent *e) {
    if(!g->enabled||e->time_ns<=g->configured_ns)return 0;
    if(e->time_ns<=g->last_event_ns){flash_cancel(g);return -1;}
    g->last_event_ns=e->time_ns;
    if(e->type==F_CAPTURE){
        g->pending=0;g->pair_wait=0;g->cycle_valid=0;g->consumed=1;
        if(g->cycle==UINT32_MAX){flash_cancel(g);return -1;}
        ++g->cycle;g->tid=e->tid;g->start_ns=e->time_ns;
        if(e->eshutter==1){g->cycle_valid=1;g->consumed=0;}return 0;
    }
    if(!g->cycle_valid||g->consumed||g->pending||e->time_ns<g->start_ns)return 0;
    if(e->time_ns-g->start_ns>FLASH_WINDOW_NS){g->cycle_valid=0;return 0;}
    if(g->source!=5&&g->source!=6&&e->tid!=g->tid)return 0;
    unsigned match=0;
    if(g->source==1&&e->type==F_VSYNC)match=e->caller==0xeea20||e->caller==0xeea44;
    if(g->source==2&&e->type==F_VSYNC)match=e->caller==0xeab78||e->caller==0xeaf5c;
    if((g->source==3&&e->type==F_TG_ENTRY)||(g->source==4&&e->type==F_CH_ENTRY)){
        if(g->pair_wait){flash_cancel(g);return -1;}
        if(e->value==1){g->pair_wait=1;g->pair_ns=e->time_ns;g->pair_tid=e->tid;g->pair_channel=e->channel;}
        return 0;
    }
    if((g->source==3&&e->type==F_TG_WRITTEN)||(g->source==4&&e->type==F_CH_WRITTEN)){
        if(!g->pair_wait)return 0;
        g->pair_wait=0;
        if(e->tid!=g->pair_tid||e->time_ns<=g->pair_ns){flash_cancel(g);return -1;}
        if(g->source==3)match=e->value==1;
        else match=e->channel==g->pair_channel&&e->channel<12&&((e->value>>e->channel)&1);
        if(!match){flash_cancel(g);return -1;}
    }
    if(g->source==5&&e->type==F_IRQ)match=e->value==0x117;
    if(g->source==6&&e->type==F_SOF)match=1;
    if(g->source==7&&e->type==F_STATE2)match=1;
    if(!match)return 0;
    if(e->time_ns>UINT64_MAX-(uint64_t)g->delay_us*1000){flash_cancel(g);return -1;}
    g->event_ns=e->time_ns;g->deadline_ns=e->time_ns+(uint64_t)g->delay_us*1000;
    g->pending=1;++g->matched;return 1;
}
static inline int flash_due(FlashCore *g,uint64_t now) {
    if(!g->enabled||!g->cycle_valid||g->consumed||!g->pending||now<g->deadline_ns)return 0;
    g->pending=0;g->consumed=1;g->late_ns=now-g->deadline_ns;
    if(g->late_ns>FLASH_LATE_NS){++g->skipped;return 0;}
    ++g->attempts;return 1; /* 先消费，输出失败不重试。 */
}
static inline int flash_uint(const char **p,unsigned *out) {
    uint64_t v=0;const char *s=*p;if(*s<'0'||*s>'9')return 0;
    do{v=v*10+(unsigned)(*s-'0');if(v>UINT32_MAX)return 0;++s;}while(*s>='0'&&*s<='9');
    *out=(unsigned)v;*p=s;return 1;
}
static inline int flash_literal(const char **p,const char *s) {
    const char *v=*p;while(*s)if(*v++!=*s++)return 0;*p=v;return 1;
}
/* 1=新配置，2=取消，3=心跳，0=不是命令，-1=坏命令。 */
static inline int flash_command(FlashCore *g,uint64_t now,const char *line) {
    const char *p=line;if(!flash_literal(&p,"X2D_SENSOR_FLASH 1 "))return 0;
    if(flash_literal(&p,"PING"))return *p?-1:3;
    unsigned seq,source,delay,on;
    if(!flash_uint(&p,&seq)||*p++!=' '||seq<=g->sequence){flash_cancel(g);return -1;}
    if(flash_literal(&p,"CANCEL")){
        flash_cancel(g);if(*p)return -1;g->sequence=seq;return 2;
    }
    if(!flash_literal(&p,"CONFIG ")||!flash_uint(&p,&source)||*p++!=' '||
       !flash_uint(&p,&delay)||*p++!=' '||!flash_uint(&p,&on)||*p){flash_cancel(g);return -1;}
    return flash_configure(g,now,seq,source,delay,on)?1:-1;
}
#endif
