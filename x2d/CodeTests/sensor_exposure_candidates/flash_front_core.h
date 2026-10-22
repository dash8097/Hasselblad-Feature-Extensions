/* R2 前按钮门控；沿用历史语义，窗口应答允许首次 EGL 初始化耗时。 */
#ifndef X2D_SENSOR_FLASH_FRONT_H
#define X2D_SENSOR_FLASH_FRONT_H
#include <stdint.h>
typedef struct {
    unsigned pressed,desired,request_seq,ack_seq,ack_state,ack_known,ready,armed,toggles,frame_seq;
    uint64_t started_ms,requested_ms;
} X2DFrontCore;
static inline void front_core_init(X2DFrontCore*f,uint64_t now){*f=(X2DFrontCore){0};f->started_ms=now;}
static inline int front_core_ack(X2DFrontCore*f,unsigned seq,unsigned state){
    if(state>1||seq!=f->request_seq||state!=f->desired)return 0;
    f->ack_seq=seq;f->ack_state=state;f->ack_known=1;f->ready=1;return 1;
}
static inline int front_core_arm(X2DFrontCore*f){
    if(!f->ready||!f->ack_known||f->request_seq||f->ack_state)return 0;
    f->armed=1;return 1;
}
static inline int front_core_event(X2DFrontCore*f,unsigned type,unsigned code,int value,uint64_t now){
    if(type!=1||code!=33)return 0;
    if(value==0){f->pressed=0;return 0;}
    if(value!=1||f->pressed)return 0;
    f->pressed=1;if(!f->armed)return 0;
    if(f->request_seq==UINT32_MAX)return -1;
    ++f->request_seq;++f->toggles;f->desired^=1;f->requested_ms=now;return 1;
}
static inline int front_core_timeout(const X2DFrontCore*f,uint64_t now){
    if(now<f->started_ms)return 1;
    if(!f->ready)return now-f->started_ms>5000;
    /* 仅页面显隐应答改为 5 s；引闪心跳仍为 2 s，超时立即撤销。 */
    return f->armed&&(!f->ack_known||f->ack_seq!=f->request_seq)&&now-f->requested_ms>5000;
}
#endif
