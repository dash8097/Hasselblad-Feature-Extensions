#ifndef X2D_SOUND_SERVICE_H
#define X2D_SOUND_SERVICE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 使用者只在所属 GUI 线程调用；init 不连接，enabled 默认为 0。 */
typedef struct {
 int ready,saving,error,attempts; /* error: 0无,1读取不可用,2保存未确认 */
 int enabled,fd,requested,connecting,initial;
 uint64_t deadline,next_read;
 size_t sent,request_size,received;
 char request[160],response[4096];
} X2dSoundService;
void x2d_sound_init(X2dSoundService*);
void x2d_sound_set_enabled(X2dSoundService*,int enabled,uint64_t now_ms);
/* 单次已观察曝光边沿调用；未知结果绝不自动重放。 */
int x2d_sound_play(X2dSoundService*,uint64_t now_ms);
void x2d_sound_poll(X2dSoundService*,uint64_t now_ms);
void x2d_sound_destroy(X2dSoundService*);
/* 纯解析函数：只接受固定服务的有界 HTTP/1.x、两字节 OK 回执、连接结束。 */
int x2d_sound_parse(const char*,size_t);
#ifdef __cplusplus
}
#endif
#endif
