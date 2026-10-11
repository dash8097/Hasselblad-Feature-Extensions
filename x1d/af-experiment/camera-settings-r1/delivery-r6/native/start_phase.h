#ifndef NA_START_PHASE_H
#define NA_START_PHASE_H
#include "native_af.h"
/* AF_SERVICE 是唯一样本/初始命令发布者；AF_SERVICE_SM 重置、结束并取一次切换。
   epoch 与计数在同一个 ARM32 原子字中，旧上下文的 CAS 不覆盖新一轮状态。 */
struct NaStartPhase {u32 word,original;};
enum { NA_START_BITS=10, NA_START_MASK=1023, NA_START_DONE=1023, NA_START_MAX_EPOCH=0x3fffff };
static inline void na_start_reset(struct NaStartPhase *p,u32 generation){
    __atomic_store_n(&p->word,generation && generation<=NA_START_MAX_EPOCH?generation<<NA_START_BITS:0,__ATOMIC_RELEASE);
}
static inline int na_start_active(const struct NaStartPhase *p,u32 generation){
    u32 v=__atomic_load_n(&p->word,__ATOMIC_ACQUIRE),count=v&NA_START_MASK;
    return generation && generation<=NA_START_MAX_EPOCH && (v>>NA_START_BITS)==generation && count>=1 && count<=501;
}
static inline int na_start_arm(struct NaStartPhase *p,u32 generation,s32 original){
    if(!generation || generation>NA_START_MAX_EPOCH || !original || original<-32767 || original>32767)return 0;
    u32 expected=generation<<NA_START_BITS;
    if(__atomic_load_n(&p->word,__ATOMIC_ACQUIRE)!=expected)return 0;
    __atomic_store_n(&p->original,original<0?(u32)-original:(u32)original,__ATOMIC_RELAXED);
    return __atomic_compare_exchange_n(&p->word,&expected,expected|1,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
}
static inline void na_start_sample(struct NaStartPhase *p){
    u32 v=__atomic_load_n(&p->word,__ATOMIC_ACQUIRE),count=v&NA_START_MASK;
    if((v>>NA_START_BITS) && count>=1 && count<501)
        (void)__atomic_compare_exchange_n(&p->word,&v,v+1,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
}
static inline void na_start_cancel(struct NaStartPhase *p){__atomic_fetch_or(&p->word,NA_START_DONE,__ATOMIC_ACQ_REL);}
static inline u32 na_start_take(struct NaStartPhase *p,u32 generation,u32 limit){
    u32 v=__atomic_load_n(&p->word,__ATOMIC_ACQUIRE),count=v&NA_START_MASK;
    if(!generation || generation>NA_START_MAX_EPOCH || (v>>NA_START_BITS)!=generation ||
       !limit || limit>500 || count<2 || count>501 || count-1<limit)return 0;
    u32 original=__atomic_load_n(&p->original,__ATOMIC_RELAXED);
    return __atomic_compare_exchange_n(&p->word,&v,(v&~NA_START_MASK)|NA_START_DONE,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)?original:0;
}
#endif
