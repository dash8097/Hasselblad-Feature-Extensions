#ifndef CAPTURE_HEADER_NATIVE_H
#define CAPTURE_HEADER_NATIVE_H
#include "capture_memory_native.h"
#include "capture_header_export.h"
typedef struct {CmNative *owner;int fd;unsigned active;} ChNative;
/* 使用已通过固定版本 bind 的 owner。只打开本进程内存用于有界 pread，
 * 错误指针返回读取失败，不直接解引用；不访问相机协议或照片。 */
int ch_native_open(ChNative *,CmNative *,ChReadApi *);
int ch_native_close(ChNative *);
#endif
