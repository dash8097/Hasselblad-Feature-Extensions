#include "native_config.h"
__attribute__((section(".data.state"),used)) struct NaConfigBank na_config_bank={0};
static int allowed(u32 x){return x==0 || x==1000 || x==2000 || x==3000 || x==5000 || x==8000 || x==12000;}
u32 na_config_checksum(const struct NaConfig *c){
    const unsigned char *p=(const unsigned char *)c;u32 h=2166136261u;
    for(u32 i=0;i<32;i++)h=(h^p[i])*16777619u;
    return h;
}
u32 na_config_validate(const struct NaConfig *c){
    if(c->magic!=NA_CONFIG_MAGIC || c->abi!=NA_CONFIG_ABI || !c->revision || (c->flags&~NA_NEW_DIRECTION))return NA_CONFIG_FORMAT;
    if(c->lens!=NA_LENS_XCD75P)return NA_CONFIG_LENS;
    if(!allowed(c->probe) || !allowed(c->fast) || !allowed(c->fine))return NA_CONFIG_SPEED;
    if(c->checksum!=na_config_checksum(c))return NA_CONFIG_CHECKSUM;
    return NA_CONFIG_OK;
}
__attribute__((section(".text.na_api"))) void na_config_reset(void){
    struct NaConfig c={NA_CONFIG_MAGIC,NA_CONFIG_ABI,NA_LENS_XCD75P,1,0,0,0,0,0};c.checksum=na_config_checksum(&c);
    na_config_bank.pending=c;na_config_bank.active=c;na_config_bank.generation=0;
    __atomic_store_n(&na_config_bank.sequence,2,__ATOMIC_RELEASE);
}
/* 单一配置接收者发布完整快照；序号保护AF开始时的读取，校验拒绝半包。 */
__attribute__((section(".text.na_api"))) u32 na_config_publish(const struct NaConfig *input){
    struct NaConfig c=*input;u32 error=na_config_validate(&c);if(error)return error;
    u32 before=__atomic_load_n(&na_config_bank.sequence,__ATOMIC_ACQUIRE);
    if(before&1)return NA_CONFIG_BUSY;
    if(c.revision<=na_config_bank.pending.revision)return NA_CONFIG_STALE;
    __atomic_store_n(&na_config_bank.sequence,before+1,__ATOMIC_RELEASE);
    na_config_bank.pending=c;
    __atomic_store_n(&na_config_bank.sequence,before+2,__ATOMIC_RELEASE);
    return NA_CONFIG_OK;
}
__attribute__((section(".text.na_api"))) u32 na_config_latch(u32 generation){
    if(!generation)return NA_CONFIG_FORMAT;
    if(generation==na_config_bank.generation)return NA_CONFIG_OK;
    for(u32 retry=0;retry<3;retry++){
        u32 before=__atomic_load_n(&na_config_bank.sequence,__ATOMIC_ACQUIRE);if(before&1)continue;
        struct NaConfig c=na_config_bank.pending;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if(before!=__atomic_load_n(&na_config_bank.sequence,__ATOMIC_ACQUIRE))continue;
        u32 error=na_config_validate(&c);if(error)return error;
        na_config_bank.active=c;na_config_bank.generation=generation;return NA_CONFIG_OK;
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
