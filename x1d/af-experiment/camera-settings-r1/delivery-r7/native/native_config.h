#ifndef NATIVE_CONFIG_H
#define NATIVE_CONFIG_H
#include "native_af.h"
enum { NA_CONFIG_MAGIC=0x34435441, NA_CONFIG_ABI=4, NA_LENS_XCD75P=75 };
enum { NA_PROBE, NA_FAST, NA_FINE };
enum { NA_NEW_DIRECTION=1, NA_FAR_FIRST=2 };
enum { NA_CONFIG_OK, NA_CONFIG_FORMAT, NA_CONFIG_LENS, NA_CONFIG_SPEED,
       NA_CONFIG_CHECKSUM, NA_CONFIG_STALE, NA_CONFIG_BUSY };
/* 速度 0 是明确的跟随原厂模式；提前量 0 则始终是绝对零毫秒。 */
enum { NA_FAST_SLOTS=4, NA_FINE_SLOTS=5, NA_ADVANCE_FACTORY=65534, NA_ADVANCE_UNSET=65535, NA_CONFIG_WORDS=13 };
struct NaConfig {
    u32 magic,abi,lens,revision,probe,fast,fine,flags;
    u32 fast_advance_ms,fine_advance_ms,start_speed,start_samples,checksum;
};
struct NaConfigBank {
    u32 sequence,generation,active_sequence;
    struct NaConfig pending,active;
};
extern struct NaConfigBank na_config_bank;
u32 na_config_checksum(const struct NaConfig *config);
u32 na_config_validate(const struct NaConfig *config);
u32 na_config_publish(const struct NaConfig *config);
u32 na_config_latch(u32 generation);
s32 na_config_command(u32 stage,s32 original);
void na_config_reset(void);
u32 na_config_snapshot(struct NaConfig *pending,struct NaConfig *active,u32 *generation);
#endif
