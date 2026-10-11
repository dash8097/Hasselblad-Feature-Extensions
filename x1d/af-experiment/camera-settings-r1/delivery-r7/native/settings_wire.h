#ifndef AF_SETTINGS_WIRE_H
#define AF_SETTINGS_WIRE_H
#include "native_config.h"
enum { AS_BYTES=255, AS_QUERY=1, AS_APPLY=2, AS_FORMAT=100, AS_CONFLICT=101, AS_APPLY_END=28+sizeof(struct NaConfig), AS_PENDING_OFFSET=44, AS_ACTIVE_OFFSET=44+sizeof(struct NaConfig) };
/* 既有 783/784 测试消息的私有定长内容；不提供地址、内存或执行入口。 */
static inline u32 as_get(const unsigned char *p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static inline void as_put(unsigned char *p,u32 n){for(u32 i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static inline u32 as_hash(const unsigned char *p,u32 bytes){u32 h=2166136261u;for(u32 i=0;i<bytes;i++)h=(h^p[i])*16777619u;return h;}
static inline int as_owned(const unsigned char *p){return as_get(p)==0x414c4248u && as_get(p+4)==0x21345346u;}
static inline int as_reply(const unsigned char *p){return as_get(p)==0x414c4248u && as_get(p+4)==0x21345246u;}
static inline void as_config_read(struct NaConfig *c,const unsigned char *p){u32 *out=(u32 *)c;for(u32 i=0;i<NA_CONFIG_WORDS;i++)out[i]=as_get(p+4*i);}
static inline void as_config_write(unsigned char *p,const struct NaConfig *c){const u32 *in=(const u32 *)c;for(u32 i=0;i<NA_CONFIG_WORDS;i++)as_put(p+4*i,in[i]);}
u32 as_process(const unsigned char request[AS_BYTES],unsigned char reply[AS_BYTES],u32 state,u32 lens);
void as_receive(const unsigned char *request);
#endif
