/* 官方 4.2.0 WriteWork 的只读头部采集候选。read 必须由可信进程内适配提供；
 * 尚未连接真实 flush 信号，不用该接口跳过原厂存储完成通知。 */
#ifndef CAPTURE_HEADER_EXPORT_H
#define CAPTURE_HEADER_EXPORT_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
 int (*read)(void *,uintptr_t address,void *out,size_t bytes);
 int (*allowed)(void *);
 void *context;
} ChReadApi;
/* 输入是已认领首帧的 RAW WriteWork；只复制前 16384 字节容器头。
 * 失败保持 out 原样。没有文件读取、像素读取、引用修改或 native 函数调用。 */
int ch_export_first_header(ChReadApi,uintptr_t work,uint8_t out[16384]);
/* 独立堆栈：外层已认领普通单帧曝光，保留一亿 RAW 几何和真实元数据。
 * 不要求像素位移组 tag 33 == 6；该函数自身不负责曝光归属。 */
int ch_export_owned_single_header(ChReadApi,uintptr_t work,uint8_t out[16384]);
#endif
