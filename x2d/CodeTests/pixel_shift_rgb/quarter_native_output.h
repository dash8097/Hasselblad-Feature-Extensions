/* 25MP 原厂后台写入适配核心。调用者是原厂输出工人，不负责曝光或快门。
 * 输入由原厂 WriteWork 强引用持有；输出也交回同一写入队列。
 * 这份核心不装载 hook、不创建第二套拍摄队列、不写磁盘。 */
#ifndef QUARTER_NATIVE_OUTPUT_H
#define QUARTER_NATIVE_OUTPUT_H
#include "quarter_memory_file.h"
typedef struct {
 unsigned width,height,stride,count;
 const RsPixelBlock *blocks;
} QnRawInput;
typedef struct {
 uint16_t *pixels;
 size_t bytes;
 unsigned width,height,stride;
} QnRawOutput;
/* 每次调用只处理当前原厂工作持有的输入，不改变其他工作。
 * 两个工作可以同时转换；所有分块线程结束后才返回给原厂写入工人。
 * 各段可在行内结束；仅跨段行使用小缓存，不复制整幅源图。
 * 各段边界、尺寸和目标范围在入口核对，不做逐像素校验。 */
int qn_raw_convert(const QnRawInput *,QnRawOutput *,unsigned workers);
typedef struct {
 uint8_t header[16384];
 uint64_t raw_offset,raw_bytes,raw_storage_bytes,preview_offset,preview_bytes,preview_storage_bytes,padding,total_bytes;
 unsigned stored_width,stored_height,effective_width,effective_height;
} QnRawPlan;
/* 先规划原厂写入片段，再提交替换；不能先让原厂写入一亿 RAW 再覆盖。
 * 预览仍由原厂工作保留，计划不复制或拥有它的引用。 */
int qn_raw_plan(const uint8_t source_header[16384],size_t preview_bytes,QnRawPlan *);
/* native 缓冲容量可能按页取整；文件 StripByteCounts 仍是有效像素字节数，
 * 预览偏移则必须按实际写入负载容量计算，不能把两者混为一谈。 */
int qn_raw_plan_storage(const uint8_t source_header[16384],size_t raw_storage_bytes,
                        size_t preview_bytes,size_t preview_storage_bytes,QnRawPlan *);
typedef struct {QnRawOutput output;void *native_owner;} QnOutputLease;
typedef struct {
 void *context;
 /* 分配的是当前工作的输出缓冲，不创建/重新打开原厂拍摄图像池。 */
 int (*allocate)(void *,size_t,QnOutputLease *);
 void (*release)(void *,QnOutputLease *);
 /* 构建新的 HeaderData/负载后一次替换；成功时移动 native_owner，清空
  * lease，保留原厂工作和完成通知。失败不能改变原有输入工作。
  * plan 为借用，回调必须复制其头部，不能保存栈地址。 */
 int (*commit)(void *,const QnRawPlan *,QnOutputLease *);
 /* 原厂工作已持有的预览负载长度；0 表示与有效预览长度相同。
  * 绑定须保证新增尾部为零，不能借此提交不属于本工作的缓存。 */
 size_t preview_storage_bytes;
} QnWriteAdapter;
/* 仅供已入队的原厂写入工人调用；不发送曝光、不停取景、不重启存储。
 * 失败只释放本次新输出；原始 RAW、预览及通知仍由原厂 WriteWork 持有。 */
int qn_raw_work_prepare(const QnRawInput *,const uint8_t source_header[16384],
                        size_t preview_bytes,QnWriteAdapter);
/* 原厂单拍在曝光入队时固定输出选择，后续切模式不改变已入队工作。 */
typedef enum { QN_NATIVE_SIZE=100, QN_QUARTER_SIZE=25 } QnResolution;
typedef struct {QnResolution resolution;unsigned effective_width,effective_height;} QnExposureOutput;
int qn_exposure_output(QnExposureOutput *,QnResolution,unsigned width,unsigned height);
#endif
