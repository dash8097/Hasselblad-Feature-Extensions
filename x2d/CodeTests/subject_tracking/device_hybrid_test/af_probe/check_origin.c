#include <assert.h>
#include <stdio.h>
#include "command_origin.h"
int main(void){
 unsigned char a[40]={10,248},b[40]={10,248};
 CommandOrigin x={.caller=10,.serial=2,.phase=5,.request=1};
 origin_put(0x1000,a,40,x);assert(origin_take(0x1000,a,40).serial==2);
 assert(origin_take(0x1000,a,40).serial==0);
 origin_put(0x1000,a,40,x);b[10]=1;assert(origin_take(0x1000,b,40).serial==0);
 origin_put(0x1000,a,40,x);origin_put(0x2000,b,40,(CommandOrigin){.serial=3});
 assert(origin_take(0x2000,b,40).serial==3);assert(origin_take(0x1000,a,40).serial==2);
 origin_put(0x1000,a,40,x);atomic_flag_test_and_set(&origins.lock);
 assert(origin_take(0x1000,a,40).serial==0);atomic_flag_clear(&origins.lock);
 assert(origin_take(0x1000,a,40).serial==0); /* 竞争后不得复用旧关联。 */
 puts("PASS: exact queue location/content, reordering, consumed/stale/contended associations rejected");
}
