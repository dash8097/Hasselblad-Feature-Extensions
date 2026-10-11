#ifndef HBL_FOCUS_CORE_H
#define HBL_FOCUS_CORE_H
#include <stdint.h>
#include <math.h>
/* No hardware access. Effects are applied by the existing Qt event loop. */
enum { FT_START=1, FT_STOP=2, FD_START=4, FD_STOP=8, F_WRITE=16, F_DONE=32, F_FAIL=64 };
typedef struct {
    uint32_t desired, sent, actual;
    int active, inFlight, failed, dragging, tickRunning;
    unsigned effects;
} FocusCore;
static double focus_displayed(const FocusCore *s) {
    return s->active || (s->dragging && !s->failed) ? s->desired : s->actual;
}
static int focus_pending(const FocusCore *s) {
    return s->active && (s->inFlight || s->desired!=s->actual);
}
static void focus_tick_start(FocusCore *s) {s->tickRunning=1;s->effects|=FT_START;}
static void focus_tick_stop(FocusCore *s) {s->tickRunning=0;s->effects|=FT_STOP;}
static void focus_select(FocusCore *s,double point) {
    if(!isfinite(point)||point<0||point>4294967295.0||floor(point)!=point)return;
    s->desired=(uint32_t)point;s->failed=0;s->active=1;
    if(!s->inFlight&&!s->tickRunning)focus_tick_start(s);
}
static void focus_complete(FocusCore *s) {
    if(s->inFlight||s->desired!=s->actual)return;
    s->active=0;focus_tick_stop(s);s->effects|=FD_STOP|F_DONE;
}
static void focus_pump(FocusCore *s) {
    if(!s->active||s->inFlight)return;
    if(s->desired==s->actual){focus_complete(s);return;}
    s->sent=s->desired;s->inFlight=1;s->effects|=FD_START|F_WRITE;
}
/* Commands: initialize/feedback, drag begin/end, select, timer, immediate pump,
   timeout, cancel timer+flush. Native code owns queue/acknowledgement decisions. */
static void focus_dispatch(FocusCore *s,int command,double point) {
    s->effects=0;
    switch(command) {
    case 0:
        if(!isfinite(point)||point<0||point>4294967295.0||floor(point)!=point)return;
        s->actual=(uint32_t)point;
        if(!s->active)return;
        if(s->inFlight&&s->actual!=s->sent)return;
        if(s->inFlight){s->inFlight=0;s->effects|=FD_STOP;}
        if(s->actual==s->desired)focus_complete(s);
        else if(!s->tickRunning)focus_tick_start(s);
        break;
    case 1:s->desired=(uint32_t)focus_displayed(s);s->dragging=1;break;
    case 2:s->dragging=0;if(!s->failed&&s->desired!=s->actual)focus_select(s,s->desired);break;
    case 3:focus_select(s,point);break;
    case 4:s->tickRunning=0;focus_pump(s);break;
    case 5:focus_pump(s);break;
    case 6:s->inFlight=0;s->active=0;s->failed=1;focus_tick_stop(s);s->effects|=FD_STOP|F_FAIL;break;
    case 7:focus_tick_stop(s);focus_pump(s);break;
    }
}
#endif
