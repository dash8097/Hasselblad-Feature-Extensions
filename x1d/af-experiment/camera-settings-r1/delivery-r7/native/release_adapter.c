#include "af_policy.h"
extern void na_cycle_reset(void),na_start_accepted(u32);
extern s32 na_stage_speed(u32,s32),na_resume_probe(s32);
void nc_cycle_reset(void){na_cycle_reset();}
void nc_probe_speed(s32 v){na_stage_speed(AP_PROBE,v);}
void nc_fast_speed(s32 v){na_stage_speed(AP_FAST,v);}
void nc_fine_speed(s32 v){na_stage_speed(AP_FINE,v);}
void nc_probe_resume(s32 v){na_resume_probe(v);}
void nc_accepted(u32 count){na_start_accepted(count);}
/* Installation-only synchronous cache acknowledgement; no task, timer, polling or hook. */
__attribute__((section(".data.state"),used)) volatile u32 af_install_ack=0;
extern void af_release_seed_current(void);
void af_install_probe(volatile u32 *ack){
    if(ack==&af_install_ack){af_release_seed_current();__atomic_store_n(ack,0x314b4341,__ATOMIC_RELEASE);}
}
