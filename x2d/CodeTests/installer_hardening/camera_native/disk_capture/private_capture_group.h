/* 拍摄归属与临时帧索引。仅后端提交自己的拍摄请求后允许认领；
 * 使用原厂实际 ExposureId 和 SnapshotMetadata，不猜测下一个编号。 */
#ifndef PRIVATE_CAPTURE_GROUP_H
#define PRIVATE_CAPTURE_GROUP_H
#include "factory_private_frame.h"
enum { X2D_CAPTURE_NORMAL, X2D_CAPTURE_OWN, X2D_CAPTURE_CONFLICT };
typedef struct {
 uint64_t job;
 uint32_t exposure_ids[16];
 unsigned frames,seen,written,active,issued,failed;
} X2dCaptureGroup;
int x2d_capture_group_arm(X2dCaptureGroup *group,uint64_t job,unsigned frames);
int x2d_capture_group_request_issued(X2dCaptureGroup *group,uint64_t job);
/* identity 缺失只能用于已认领 ExposureId 的后续通知。 */
int x2d_capture_group_classify(X2dCaptureGroup *group,uint32_t exposure_id,
 const X2dCaptureIdentity *identity,unsigned *index);
int x2d_capture_group_written(X2dCaptureGroup *group,unsigned index,uint32_t exposure_id);
/* 所有图像与文件引用结束且原厂曝光空闲后，才能解除归属。 */
int x2d_capture_group_close(X2dCaptureGroup *group,uint64_t job,
 int native_idle,int references_released);
#endif
