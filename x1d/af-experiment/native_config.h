#ifndef NATIVE_CONFIG_H
#define NATIVE_CONFIG_H
#include "native_af.h"
enum { NA_CONFIG_MAGIC=0x31435441, NA_CONFIG_ABI=1, NA_LENS_XCD75P=75 };
enum { NA_PROBE, NA_FAST, NA_FINE };
enum { NA_NEW_DIRECTION=1 };
enum { NA_CONFIG_OK, NA_CONFIG_FORMAT, NA_CONFIG_LENS, NA_CONFIG_SPEED,
       NA_CONFIG_CHECKSUM, NA_CONFIG_STALE, NA_CONFIG_BUSY };
/* 0 跟随本轮原厂命令；其他值是有符号16位协议的幅值，不是 RPM。 */
struct NaConfig { u32 magic,abi,lens,revision,probe,fast,fine,flags,checksum; };
struct NaConfigBank {
    u32 sequence,generation;
    struct NaConfig pending,active;
};
extern struct NaConfigBank na_config_bank;
u32 na_config_checksum(const struct NaConfig *config);
u32 na_config_validate(const struct NaConfig *config);
u32 na_config_publish(const struct NaConfig *config);
u32 na_config_latch(u32 generation);
s32 na_config_command(u32 stage,s32 original);
void na_config_reset(void);
#endif
