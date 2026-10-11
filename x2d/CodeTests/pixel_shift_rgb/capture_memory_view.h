/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 项目自定义的借用字节视图，不对应任何厂商对象或共享指针 ABI。
 * format=1 表示小端无符号 16 位单通道样本；stride 按字节计。
 * 生命周期由调用者负责，计算期间不得释放或改写数据。 */
#ifndef CAPTURE_MEMORY_VIEW_H
#define CAPTURE_MEMORY_VIEW_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {
 uint32_t width,height,stride;
 uint8_t format;
 const uint8_t *data;
 size_t bytes;
 int owns_allocation;
} CmView;
#endif
