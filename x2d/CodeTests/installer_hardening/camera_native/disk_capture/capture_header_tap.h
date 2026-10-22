/* 在已认领首帧的真实 WriteWork 回调中导出头部；原厂写入照常完成。
 * 不是全局通知 hook，外层负责注册本任务拍摄入口和 pump。 */
#ifndef CAPTURE_HEADER_TAP_H
#define CAPTURE_HEADER_TAP_H
#include "capture_header_export.h"
#include "factory_private_frame.h"
typedef struct {
 ChReadApi read;
 X2dPrivateFrame *frame;
 uint8_t header[16384];
 unsigned attached,ready,failed;
} ChTap;
/* tap 首次使用须零初始化，生命周期须长于 frame 的信号连接。 */
int ch_tap_attach(ChTap *,X2dPrivateFrame *,const X2dCaptureIdentity *,ChReadApi);
#endif
