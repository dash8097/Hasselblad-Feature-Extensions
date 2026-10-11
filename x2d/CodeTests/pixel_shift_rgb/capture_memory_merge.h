/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 仅计算像素的分块内存合成。六个 view 在整个调用/计算线程 join 前保持租约。
 * 输出为本块 GRBG RAW，不创建 DNG、3FR、JPEG 或任何文件。 */
#ifndef CAPTURE_MEMORY_MERGE_H
#define CAPTURE_MEMORY_MERGE_H
#include "capture_memory_view.h"
typedef void (*CmMergeDispatch)(void *context,unsigned width,
 void (*range)(unsigned,unsigned,void *),void *row);
size_t cm_merge_scratch_bytes(unsigned width);
/* rows 指源行数，输出行数是 2*rows；至多 1024 源行一块。
 * dispatch 返回前必须 join 所有区间。为 NULL 时单线程计算。 */
int cm_merge_rows(const CmView frames[6],unsigned width,unsigned height,
 unsigned first_row,unsigned rows,uint16_t *output,size_t output_bytes,
 void *scratch,size_t scratch_bytes,CmMergeDispatch dispatch,void *context);
#endif
