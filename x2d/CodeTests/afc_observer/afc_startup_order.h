#ifndef AFC_STARTUP_ORDER_H
#define AFC_STARTUP_ORDER_H
/* X2D 4.2.0: startup-only order: base AF -> observer -> settle -> phase.
   Missing enabled predecessors leave hooks detached. No AF thread waits. */
#include "afc_center_startup_gate.h"
extern long read(int,void*,U);
__attribute__((noinline)) static int afc_predecessors_ready(unsigned level){
    static const struct {const char*env;const char*path;} peers[]={
        {"X2D_AF_PROBE","/tmp/x2d-af-probe.json"},
        {"X2D_AFC_OBSERVER","/tmp/x2d-afc-observer.json"},
        {"X2D_AFC_SETTLE_33MS","/tmp/x2d-afc-settle.json"}
    };
    if(!level||level>3)return 0;
    int pid=getpid();char text[512];
    for(unsigned i=0;i<level;++i){
        const char*on=getenv(peers[i].env);
        if(!on||on[0]!='1'||on[1])continue;
        int fd=open(peers[i].path,0xA0000);if(fd<0)return 0;
        long n=read(fd,text,sizeof text);close(fd);
        if(n<=0||n>(long)sizeof text||!afc_center_peer_header(text,(unsigned)n,pid))return 0;
    }
    return 1;
}
__attribute__((noinline)) static int afc_wait_predecessors(unsigned level){
    for(unsigned i=0;i<120;++i){
        if(afc_predecessors_ready(level))return 1;
        struct TS delay={0,250000000};nanosleep(&delay,0);
    }
    return 0;
}
#endif
