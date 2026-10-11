/* 与 ARM 共用判向/预判和配置实现；浏览器仅运行合成模型，不含设备接口。 */
#include "native_config.h"
#define UI_EXPORT(name) __attribute__((export_name(name)))
static struct NaSample ui_samples[5];
static struct NaTiming ui_timing;
static struct NaResult ui_result;
static struct NaConfig ui_config;
static const u32 ui_layout[]={sizeof(struct NaSample),sizeof(struct NaTiming),sizeof(struct NaResult),sizeof(struct NaConfig)};
UI_EXPORT("layout_ptr") u32 ui_layout_ptr(void){return (u32)ui_layout;}
UI_EXPORT("samples_ptr") u32 ui_samples_ptr(void){return (u32)ui_samples;}
UI_EXPORT("timing_ptr") u32 ui_timing_ptr(void){return (u32)&ui_timing;}
UI_EXPORT("result_ptr") u32 ui_result_ptr(void){return (u32)&ui_result;}
UI_EXPORT("config_ptr") u32 ui_config_ptr(void){return (u32)&ui_config;}
UI_EXPORT("bank_ptr") u32 ui_bank_ptr(void){return (u32)&na_config_bank;}
UI_EXPORT("assess") void ui_assess(u32 n){na_assess(ui_samples,n,&ui_timing,&ui_result);}
UI_EXPORT("config_reset") void ui_config_reset(void){na_config_reset();}
UI_EXPORT("config_checksum") u32 ui_config_checksum(void){return na_config_checksum(&ui_config);}
UI_EXPORT("config_publish") u32 ui_config_publish(void){return na_config_publish(&ui_config);}
UI_EXPORT("config_latch") u32 ui_config_latch(u32 generation){return na_config_latch(generation);}
UI_EXPORT("stage_command") s32 ui_stage_command(u32 stage,s32 original){return na_config_command(stage,original);}
