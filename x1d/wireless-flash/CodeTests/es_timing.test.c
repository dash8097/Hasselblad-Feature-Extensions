#include "../native/rf_es_timing.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    unsigned delay=1234;
    assert(rf_es_center_delay(250000,0,&delay) && delay==318);
    assert(rf_es_center_delay(166667,0,&delay) && delay==359);
    assert(rf_es_center_delay(125000,0,&delay) && delay==380);
    assert(rf_es_center_delay(300000,0,&delay) && delay==593);
    assert(rf_es_center_delay(400000,0,&delay) && delay==543);
    assert(rf_es_center_delay(500000,0,&delay) && delay==493);
    assert(!rf_es_center_delay(500001,0,&delay));
    assert(!rf_es_center_delay(0,0,&delay));
    assert(!rf_es_center_delay(250000,1,&delay));
    assert(!rf_es_center_delay(270000,0,&delay));
    assert(!rf_es_center_delay(250000,0,0));
    for(uint32_t t=1;t<=500000;t++) {
        int valid=rf_es_center_delay(t,0,&delay);
        assert(valid==(t<=250000 || t>=295000));
        if(!valid) continue;
        int64_t twice_center=(int64_t)delay*2000;
        int64_t expected=(t<=250000 ? 1180000 : 1780000)-295000-t;
        assert(twice_center-expected>=-1000 && twice_center-expected<=1000);
    }
    RfCore core; rf_init(&core,1); rf_configure(&core,1,0,0);
    RfEvent begin={RF_BEGIN,1,1,0,1,1},phase={RF_PHASE,1,1,0,0,2};
    assert(rf_event(&core,&begin,1000));
    assert(!rf_es_set_deadline(&core,318,1000));
    assert(rf_event(&core,&phase,1000));
    assert(rf_es_set_deadline(&core,318,1000));
    assert(core.delay_ms==0 && core.deadline_ms==1318);
    assert(!rf_due(&core,1317) && rf_due(&core,1318) && !rf_due(&core,1318));
    assert(!rf_es_set_deadline(&core,318,1000));
    begin.shot=2; begin.sequence=3; phase.shot=2; phase.sequence=4;
    assert(rf_event(&core,&begin,2000) && rf_event(&core,&phase,2000));
    assert(rf_es_set_deadline(&core,493,2000));
    rf_configure(&core,0,0,0); assert(!rf_due(&core,2493));
    puts("es timing: examples, exclusions and 500000 exposure values passed; hardware=0");
    return 0;
}
