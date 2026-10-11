/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#include <stdio.h>
#include "pixel_mode_events.h"
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
int main(void){
 unsigned char events[65]={0};uint32_t h[4]={1,8,0,16};int changed=0;
 memcpy(events+1,h,16);memcpy(events+17,"status.tmp",11);
 CHECK(po_arm_event(events+1,32,&changed)&&!changed);
 memcpy(events+17,"arm.txt",8);
 CHECK(po_arm_event(events+1,32,&changed)&&changed);
 CHECK(!po_arm_event(events+1,31,&changed));
 memset(events+17,'a',16);CHECK(!po_arm_event(events+1,32,&changed));
 h[1]=0x4000;h[3]=0;memcpy(events+1,h,16);
 CHECK(po_arm_event(events+1,16,&changed)&&changed);
 CHECK(!po_arm_event(events+1,15,&changed));
 CHECK(po_arm_event(events+1,0,&changed)&&!changed);
 puts("MODE_EVENTS_UNALIGNED_NAMES_TRUNCATION_AND_OVERFLOW_PASSED");return 0;
}
