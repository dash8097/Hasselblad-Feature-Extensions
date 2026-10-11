#include "../native/rf_core.h"
#include <assert.h>
#include <stdio.h>
static RfEvent event(uint32_t kind,uint32_t shot,uint32_t phase,uint32_t seq) {
    RfEvent e={kind,77,shot,phase,1,seq}; return e;
}
int main(void) {
    RfCore s; RfEvent e,d; unsigned char bytes[32]; int count=0;
    const unsigned delays[]={0,20,5000};
    for(unsigned index=0;index<3;index++) {
        unsigned delay=delays[index];
        rf_init(&s,77); assert(rf_configure(&s,1,0,delay));
        e=event(RF_BEGIN,1,0,1); assert(rf_event(&s,&e,100));
        e=event(RF_PHASE,1,0,2); assert(rf_event(&s,&e,101));
        if(delay) assert(!rf_due(&s,100+delay));
        assert(rf_due(&s,101+delay));
        assert(!rf_due(&s,102+delay)); count++;
    }
    for(unsigned stop=0;stop<8;stop++) {
        rf_init(&s,77); rf_configure(&s,1,0,100);
        e=event(RF_BEGIN,1,0,1); rf_event(&s,&e,10);
        e=event(RF_PHASE,1,0,2); rf_event(&s,&e,11);
        if(stop==0) rf_configure(&s,0,0,100);
        if(stop==1) { e=event(RF_HELLO,0,0,3); rf_event(&s,&e,12); }
        if(stop==2) rf_configure(&s,1,0,101);
        if(stop==3) { e=event(RF_END,1,0,3); rf_event(&s,&e,12); }
        if(stop==4) { e=event(RF_BEGIN,2,0,3); rf_event(&s,&e,12); }
        if(stop==5) { e=event(RF_PHASE,1,0,4); rf_event(&s,&e,12); }
        if(stop==6) rf_cancel(&s);
        if(stop==7) assert(!rf_due(&s,9));
        assert(!rf_due(&s,111)); count++;
    }
    rf_init(&s,77);
    for(unsigned source=3;source<=6;++source) { assert(!rf_configure(&s,1,source,0)); count++; }
    for(unsigned source=0;source<3;++source) {
        rf_init(&s,77); assert(rf_configure(&s,1,source,0));
        e=event(RF_BEGIN,1,0,1); assert(rf_event(&s,&e,100));
        for(unsigned phase=0;phase<3;++phase) {
            e=event(RF_PHASE,1,phase,phase+2); assert(rf_event(&s,&e,101+phase));
            assert(rf_due(&s,101+phase)==(phase==source));
        }
        assert(!rf_due(&s,104)); count++;
    }
    rf_init(&s,77); rf_configure(&s,1,1,0);
    e=event(RF_BEGIN,1,0,1); rf_event(&s,&e,100);
    e=event(RF_PHASE,1,0,2); rf_event(&s,&e,101);
    rf_configure(&s,1,2,0);
    e=event(RF_PHASE,1,2,3); assert(!rf_event(&s,&e,102)); assert(!rf_due(&s,102)); count++;
    assert(!rf_configure(&s,1,0,5001)); count++;
    rf_configure(&s,1,0,0); e=event(RF_BEGIN,1,0,1); e.detail=0;
    rf_event(&s,&e,0); e=event(RF_PHASE,1,0,2); rf_event(&s,&e,1); assert(!rf_due(&s,1)); count++;
    rf_init(&s,77); rf_configure(&s,1,0,0); e=event(RF_BEGIN,1,0,1); rf_event(&s,&e,0);
    e=event(RF_PHASE,1,0,2); rf_event(&s,&e,1); assert(!rf_due(&s,252)); count++;
    for(unsigned phase=3;phase<=6;++phase) {
        e=event(RF_PHASE,3,phase,15); rf_encode(bytes,&e); assert(!rf_decode(bytes,32,&d)); count++;
    }
    e=event(RF_PHASE,3,0,15); rf_encode(bytes,&e); assert(rf_decode(bytes,32,&d));
    assert(d.shot==3 && d.phase==0 && d.sequence==15);
    for(unsigned i=0;i<32;i++) { bytes[i]^=1; assert(!rf_decode(bytes,32,&d)); bytes[i]^=1; count++; }
    assert(!rf_decode(bytes,31,&d)); assert(!rf_decode(bytes,33,&d)); count++;
    rf_init(&s,77); rf_configure(&s,1,0,0); e=event(RF_BEGIN,1,0,1); rf_event(&s,&e,10);
    e=event(RF_PHASE,2,0,2); assert(!rf_event(&s,&e,11)); assert(!rf_due(&s,11)); count++;
    e=event(RF_PHASE,1,0,2); assert(!rf_event(&s,&e,12)); assert(!rf_due(&s,12)); count++;
    e=event(RF_PHASE,1,0,3); e.session=78; assert(!rf_event(&s,&e,13)); count++;
    for(unsigned threshold=0;threshold<=100;++threshold) {
        rf_init(&s,77); rf_configure(&s,1,1,20); assert(rf_progress_configure(&s,threshold));
        e=event(RF_BEGIN,1,0,1); assert(rf_event(&s,&e,100));
        e=event(RF_PHASE,1,0,2); assert(rf_event(&s,&e,101));
        unsigned fired=0,seq=2;
        for(unsigned value=0;value<=100;value+=2) {
            e=event(RF_PHASE,1,1,++seq); e.detail=value;
            rf_event(&s,&e,102+value);
            if(s.pending) { assert(value>=threshold); assert(rf_due(&s,122+value)); ++fired; break; }
            assert(value<threshold);
        }
        assert(fired==1); assert(!rf_due(&s,500)); count++;
    }
    rf_init(&s,77); rf_configure(&s,1,1,0); rf_progress_configure(&s,50);
    e=event(RF_BEGIN,1,0,1); rf_event(&s,&e,100);
    e=event(RF_PHASE,1,1,2); e.detail=20; rf_event(&s,&e,101);
    rf_progress_configure(&s,10);
    e.sequence=3; assert(!rf_event(&s,&e,102)); assert(!rf_due(&s,102)); count++;
    assert(!rf_progress_configure(&s,101)); count++;
    printf("native offline checks: %d; hardware requests: 0\n",count); return 0;
}
