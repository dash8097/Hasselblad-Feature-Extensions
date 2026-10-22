#include "near_guard.h"
#include "near_hooks.h"
static NearGuard near_policy;
static atomic_flag near_busy=ATOMIC_FLAG_INIT;
static atomic_uint near_active,near_contention;
#ifdef X2D_AFC_NEAR_HOLD
static atomic_uint near_afc_mode;
#endif
static void near_reset(unsigned active) {
    atomic_store_explicit(&near_active,0,memory_order_release);
    if(atomic_flag_test_and_set_explicit(&near_busy,memory_order_acquire)) {
        atomic_fetch_add_explicit(&near_contention,1,memory_order_relaxed);return;
    }
    near_clear(&near_policy);
    atomic_store_explicit(&near_active,active,memory_order_release);
    atomic_flag_clear_explicit(&near_busy,memory_order_release);
}
static int near_eligible(unsigned c) {
    if(c||!atomic_load_explicit(&near_active,memory_order_acquire)||!speed_allowed())return 0;
    if(((int(*)(unsigned))(base+NEAR_RECORDING))(c))return 0;
    unsigned afc=((int(*)(unsigned))(base+NEAR_AFC))(c)!=0;
#ifdef X2D_AFC_NEAR_HOLD
    /* 测试机候选：切换模式不继承上一模式的慢档保持。 */
    if(atomic_exchange_explicit(&near_afc_mode,afc,memory_order_acq_rel)!=afc)
        near_reset(atomic_load_explicit(&near_active,memory_order_relaxed));
    return 1;
#else
    return !afc;
#endif
}
static int near_lock(void) {
    if(!atomic_flag_test_and_set_explicit(&near_busy,memory_order_acquire))return 1;
    atomic_fetch_add_explicit(&near_contention,1,memory_order_relaxed);return 0;
}
static void near_unlock(void) {atomic_flag_clear_explicit(&near_busy,memory_order_release);}
/* 这两个调用点刚执行过 get_pdaf_distance_to_peak。
 * value=0 也可能是门槛没通过；必须重核完全相同的门槛，不能把未知当作远。 */
static int near_compared(unsigned c,U caller) {
    unsigned off=caller==0x904d8?0x57:caller==0x905c0?0x5a:0;
    if(!off)return 0;
    const unsigned char *p=((const unsigned char*(*)(unsigned))(base+NEAR_PDAF_PARAM))(c);
    const unsigned char *m=((const unsigned char*(*)(unsigned))(base+NEAR_MODE_PARAM))(c);
    return p&&m&&m[0x28]&&(int)(int8_t)p[off]>(int)m[0x28];
}
static void near_set_distance(unsigned c,unsigned alg,int value) {
    U caller=relative((U)__builtin_return_address(0));int effective=value,previous=0,released=0;
    if(alg==1&&(caller==0x904d8||caller==0x905c0||caller==0x90720)&&near_eligible(c)) {
        int compared=value==0&&near_compared(c,caller);
        unsigned frame=((unsigned(*)(unsigned))(base+NEAR_FRAME))(c);
        previous=((int(*)(unsigned,unsigned))(base+NEAR_DISTANCE))(c,alg);
        if(near_lock()) {
            unsigned was=near_policy.slow;
#ifdef X2D_AFC_NEAR_HOLD
            /* AFC 恢复快档要求近邻新帧；长间隔或帧号倒退不能拼两次确认。 */
            if(atomic_load_explicit(&near_afc_mode,memory_order_relaxed)&&
               near_policy.have_far_frame&&frame-near_policy.far_frame>3) {
                near_policy.far_count=0;near_policy.have_far_frame=0;
            }
#endif
            if(atomic_load_explicit(&near_active,memory_order_acquire))
                effective=near_distance(&near_policy,value,previous,compared,frame);
            released=was&&!near_policy.slow;near_unlock();
        }
    } else if(c==0)near_reset(atomic_load_explicit(&near_active,memory_order_relaxed));
    ((void(*)(unsigned,unsigned,int))originals[20])(c,alg,effective);
    if(effective!=value||released)af_record((AfEvent){.us=now(),.caller=caller,.kind=AF_NEAR_GUARD,
        .context=(int)c,.a=value,.b=effective,.flags=released?3:2,.phase=-1});
}
static unsigned near_select_speed(unsigned c) {
    U caller=relative((U)__builtin_return_address(0));
    unsigned proposed=((unsigned(*)(unsigned))originals[21])(c),effective=proposed;
    if(near_eligible(c)) {
        if(near_lock()) {
            if(atomic_load_explicit(&near_active,memory_order_acquire))effective=near_speed(&near_policy,proposed);
            near_unlock();
        }
    } else if(c==0)near_reset(atomic_load_explicit(&near_active,memory_order_relaxed));
    if(effective!=proposed) {
        /* 原选速函数已保存 proposed；同步保存实际采用档位，清除旧提速等待。 */
        ((void(*)(unsigned,unsigned))(base+NEAR_CURRENT_TYPE))(c,effective);
        ((void(*)(unsigned,unsigned))(base+NEAR_WAIT_FRAME))(c,0);
        af_record((AfEvent){.us=now(),.caller=caller,.kind=AF_NEAR_GUARD,.context=(int)c,
            .a=(int)proposed,.b=(int)effective,.flags=1,.phase=-1});
    }
    return effective;
}
static U near_reset_assistant(unsigned c) {
    if(c==0)near_reset(atomic_load_explicit(&near_active,memory_order_relaxed));
    return ((U(*)(unsigned))originals[22])(c);
}
