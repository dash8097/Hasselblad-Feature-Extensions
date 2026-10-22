/* 官方 4.2.0 ImageMemory 借用接口；候选库不自动装载、不修改任何虚表。 */
#ifndef CAPTURE_MEMORY_NATIVE_H
#define CAPTURE_MEMORY_NATIVE_H
#include "capture_memory_input.h"
typedef struct {X2dPrivateNativeApi api;void *storage_owner;} CmNative;
/* storage_owner 必须是当前存储线程的真实 QObject。bind 核对代码指纹。 */
int cm_native_bind(CmNative *native,uintptr_t storage_base,void *storage_owner,CmApi *out);
#endif
