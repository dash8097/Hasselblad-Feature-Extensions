#include "native_config.h"
__attribute__((section(".data.state"),used)) struct NaConfigBank na_config_bank={0};
static void load_config(struct NaConfig *to,const struct NaConfig *from){
    u32 *d=(u32 *)to;const u32 *s=(const u32 *)from;
    for(u32 i=0;i<NA_CONFIG_WORDS;i++)d[i]=__atomic_load_n(s+i,__ATOMIC_RELAXED);
}
static void store_config(struct NaConfig *to,const struct NaConfig *from){
    u32 *d=(u32 *)to;const u32 *s=(const u32 *)from;
    for(u32 i=0;i<NA_CONFIG_WORDS;i++)__atomic_store_n(d+i,s[i],__ATOMIC_RELAXED);
}
u32 na_config_checksum(const struct NaConfig *c){
    const unsigned char *p=(const unsigned char *)c;u32 h=2166136261u;
    for(u32 i=0;i<sizeof(*c)-4;i++)h=(h^p[i])*16777619u;
    return h;
}
u32 na_config_validate(const struct NaConfig *c){
    if(c->magic!=NA_CONFIG_MAGIC || c->abi!=NA_CONFIG_ABI || !c->revision || c->revision>0x7fffffffu ||
       (c->flags&~(NA_NEW_DIRECTION|NA_FAR_FIRST)))return NA_CONFIG_FORMAT;
    if(c->lens!=NA_LENS_XCD75P)return NA_CONFIG_LENS;
    if((c->probe && (c->probe<5000 || c->probe>13000 || (c->probe-5000)%2000)) ||
       (c->fast && (c->fast<5000 || c->fast>20000 || c->fast%5000)) ||
       (c->fine && (c->fine<3000 || c->fine>7000 || c->fine%1000)))return NA_CONFIG_SPEED;
    if((c->fast_advance_ms>200 && c->fast_advance_ms<NA_ADVANCE_FACTORY) || c->fast_advance_ms>NA_ADVANCE_UNSET ||
       (c->fine_advance_ms>200 && c->fine_advance_ms<NA_ADVANCE_FACTORY) || c->fine_advance_ms>NA_ADVANCE_UNSET)return NA_CONFIG_FORMAT;
    if(c->checksum!=na_config_checksum(c))return NA_CONFIG_CHECKSUM;
    return NA_CONFIG_OK;
}
__attribute__((section(".text.na_api"))) void na_config_reset(void){
    struct NaConfig c={.magic=NA_CONFIG_MAGIC,.abi=NA_CONFIG_ABI,.lens=NA_LENS_XCD75P,.revision=1,
                       .probe=0,.fast=0,.fine=0,.flags=NA_FAR_FIRST,
                       .fast_advance_ms=NA_ADVANCE_FACTORY,.fine_advance_ms=NA_ADVANCE_FACTORY};
    c.checksum=na_config_checksum(&c);
    na_config_bank.pending=c;na_config_bank.active=c;na_config_bank.generation=0;
    na_config_bank.active_sequence=2;
    __atomic_store_n(&na_config_bank.sequence,2,__ATOMIC_RELEASE);
}
/* 单一配置接收者发布完整快照；序号保护AF开始时的读取，校验拒绝半包。 */
__attribute__((section(".text.na_api"))) u32 na_config_publish(const struct NaConfig *input){
    struct NaConfig c=*input;u32 error=na_config_validate(&c);if(error)return error;
    u32 before=__atomic_load_n(&na_config_bank.sequence,__ATOMIC_ACQUIRE);
    if((before&1) || before>0xfffffffcu)return NA_CONFIG_BUSY;
    if(!__atomic_compare_exchange_n(&na_config_bank.sequence,&before,before+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return NA_CONFIG_BUSY;
    if(c.revision<=na_config_bank.pending.revision){__atomic_store_n(&na_config_bank.sequence,before+2,__ATOMIC_RELEASE);return NA_CONFIG_STALE;}
    store_config(&na_config_bank.pending,&c);
    __atomic_store_n(&na_config_bank.sequence,before+2,__ATOMIC_RELEASE);
    return NA_CONFIG_OK;
}
__attribute__((section(".text.na_api"))) u32 na_config_latch(u32 generation){
    if(!generation)return NA_CONFIG_FORMAT;
    if(generation==na_config_bank.generation)return NA_CONFIG_OK;
    for(u32 retry=0;retry<3;retry++){
        u32 before=__atomic_load_n(&na_config_bank.sequence,__ATOMIC_ACQUIRE);if(before&1)continue;
        struct NaConfig c;load_config(&c,&na_config_bank.pending);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if(before!=__atomic_load_n(&na_config_bank.sequence,__ATOMIC_ACQUIRE))continue;
        u32 error=na_config_validate(&c);if(error)return error;
        u32 active=__atomic_load_n(&na_config_bank.active_sequence,__ATOMIC_ACQUIRE);
        __atomic_store_n(&na_config_bank.active_sequence,active+1,__ATOMIC_RELEASE);
        store_config(&na_config_bank.active,&c);__atomic_store_n(&na_config_bank.generation,generation,__ATOMIC_RELAXED);
        __atomic_store_n(&na_config_bank.active_sequence,active+2,__ATOMIC_RELEASE);
        return NA_CONFIG_OK;
    }
    return NA_CONFIG_BUSY;
}
u32 na_config_snapshot(struct NaConfig *pending,struct NaConfig *active,u32 *generation){
    for(u32 retry=0;retry<3;retry++){
        u32 p=__atomic_load_n(&na_config_bank.sequence,__ATOMIC_ACQUIRE);
        u32 a=__atomic_load_n(&na_config_bank.active_sequence,__ATOMIC_ACQUIRE);
        if((p|a)&1)continue;
        load_config(pending,&na_config_bank.pending);load_config(active,&na_config_bank.active);*generation=__atomic_load_n(&na_config_bank.generation,__ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if(p==__atomic_load_n(&na_config_bank.sequence,__ATOMIC_ACQUIRE) && a==__atomic_load_n(&na_config_bank.active_sequence,__ATOMIC_ACQUIRE))
            return na_config_validate(pending)|na_config_validate(active);
    }
    return NA_CONFIG_BUSY;
}
__attribute__((section(".text.na_api"))) s32 na_config_command(u32 stage,s32 original){
    if(!original || original<-32768 || original>32767 || stage>NA_FINE)return original;
    const struct NaConfig *c=&na_config_bank.active;
    if(na_config_validate(c))return original;
    u32 choice=stage==NA_PROBE?c->probe:(stage==NA_FAST?c->fast:c->fine);
    if(!choice)return original;
    return original<0?-(s32)choice:(s32)choice;
}
