#include "probe_core.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 AfSnapshot s;
 af_record((AfEvent){.kind=AF_BEGIN,.us=100});
 af_record((AfEvent){.kind=AF_SPEED,.a=2,.b=2345});
 assert(af_snapshot(&s)&&s.session==1&&s.count==2&&s.events[1].b==2345);
 atomic_flag_test_and_set(&af_buffer.busy);
 af_record((AfEvent){.kind=AF_SCAN_QUEUE});
 assert(!af_snapshot(&s));
 atomic_flag_clear(&af_buffer.busy);
 assert(af_snapshot(&s)&&s.dropped==1&&s.count==2);
 for(int i=0;i<AF_EVENT_CAPACITY+10;i++)af_record((AfEvent){.kind=AF_BRANCH});
 assert(af_snapshot(&s)&&s.count==AF_EVENT_CAPACITY&&s.dropped==13);
 af_record((AfEvent){.kind=AF_BEGIN,.context=1});
 assert(af_snapshot(&s)&&s.session==2&&s.count==1&&!s.dropped);
 puts("PASS: bounded capacity, contention drops, session reset, value preservation");
 return 0;
}
