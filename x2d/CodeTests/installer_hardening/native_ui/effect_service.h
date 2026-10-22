#ifndef X2D_EFFECT_SERVICE_H
#define X2D_EFFECT_SERVICE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 使用者只在所属 GUI 线程调用；init 不连接，enabled 默认为 0。 */
typedef struct {
 int mode,ready,saving,error,attempts; /* error: 0无,1读取不可用,2保存未确认 */
 int enabled,fd,requested,connecting,initial;
 uint64_t deadline,next_read;
 size_t sent,request_size,received;
 char request[160],response[4096];
} X2dEffectService;
void x2d_effect_init(X2dEffectService*);
void x2d_effect_set_enabled(X2dEffectService*,int enabled,uint64_t now_ms);
int x2d_effect_select(X2dEffectService*,int mode,uint64_t now_ms);
void x2d_effect_poll(X2dEffectService*,uint64_t now_ms);
void x2d_effect_destroy(X2dEffectService*);
/* 纯解析函数：只接受固定服务的有界 HTTP/1.x、单字节模式、连接结束。 */
int x2d_effect_parse(const char*,size_t);
#ifdef __cplusplus
}
#endif
#endif
