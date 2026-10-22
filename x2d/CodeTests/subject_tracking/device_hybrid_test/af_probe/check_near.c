#include <assert.h>
#include <stdio.h>
#include "near_guard.h"
int main(void) {
 NearGuard s={0};
 assert(near_speed(&s,0)==0); /* 初始粗搜不被限制。 */
 assert(near_speed(&s,1)==1);assert(near_speed(&s,0)==1);
 assert(near_speed(&s,2)==2);assert(near_speed(&s,0)==2);
 assert(near_speed(&s,1)==1);assert(near_speed(&s,0)==1);
 assert(near_distance(&s,2,0,0,100)==2);
 assert(near_distance(&s,0,2,0,101)==2);assert(near_speed(&s,0)==2);
 assert(near_distance(&s,0,2,1,102)==2);
 assert(near_distance(&s,0,2,1,102)==2); /* 同帧重复不解除。 */
 assert(near_speed(&s,0)==2);
 assert(near_distance(&s,0,2,1,103)==0);assert(near_speed(&s,0)==0);
 assert(near_distance(&s,1,0,0,104)==1);
 assert(near_distance(&s,0,1,1,105)==1);
 assert(near_distance(&s,0,1,0,106)==1); /* 未知中断连续远距证据。 */
 assert(near_distance(&s,0,1,1,107)==1);assert(near_speed(&s,0)==1);
 assert(near_distance(&s,2,1,0,108)==2);assert(near_speed(&s,0)==2);
 near_clear(&s);assert(near_speed(&s,0)==0);
 assert(near_distance(&s,0,0,0,109)==0);assert(near_speed(&s,0)==0);
 assert(near_speed(&s,3)==3&&near_speed(&s,4)==4&&near_speed(&s,5)==5);
 assert(near_speed(&s,6)==6&&near_speed(&s,999)==999);
 puts("PASS: initial fast, 1/2-to-0 guard, direct 0-to-2, valid far release across distinct frames, unknown/reset and non-still types");
}
