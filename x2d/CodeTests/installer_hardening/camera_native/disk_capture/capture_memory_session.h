/* 离线集成候选：所有控制入口在 CmApi.allowed 指定的 owner 线程调用。
 * 不注册 hook、不发曝光、不确认原厂存储通知、不登记或删除相册。
 * 两个 consume 回调运行在工作线程，必须同步完成；不可直接调用 QObject。
 * 真实头部导出与原厂显影跨线程适配未验证，不可直接作为安装入口。 */
#ifndef CAPTURE_MEMORY_SESSION_H
#define CAPTURE_MEMORY_SESSION_H
#include "capture_memory_pipeline.h"
#include "capture_output_workers.h"
typedef struct CsSession CsSession;
typedef struct {
 int (*begin)(void *,const uint8_t *preview,size_t bytes,unsigned width,unsigned height,
              CwConsumer consumers[PC_CONSUMERS]);
 int (*finish)(void *);
 void (*abort)(void *);
 void *context;
} CsOutputApi;
typedef struct {
 uint64_t task,timeout_ms;
 unsigned width,height,batch;
 size_t working_limit; /* 仅协调层：双缓冲、scratch、预览；不含原厂池和消费者。 */
 CmApi input;
 CsOutputApi output;
 CmMergeDispatch dispatch;
 void *dispatch_context;
 unsigned compute_threads; /* 0/1 为顺序基线，4 为持久按行线程池。 */
 unsigned compact_output; /* 串行文件显影：无额外周边行。 */
 unsigned compact_buffers; /* 0/1 为单缓冲，2 为计算与后台写盘轮换。 */
 unsigned cache_rows; /* 0 使用批大小上限；否则限制六帧输入缓存的行数。 */
 unsigned full_output; /* 外部共享池持有整幅结果；先算完、归还源图，再并行消费。 */
 uint16_t *(*full_pixels)(void *,size_t);
 int (*sources_released)(void *);
} CsConfig;
enum {CS_REJECTED=-2,CS_FAILED=-1,CS_WAIT=0,CS_PROGRESS=1,CS_COMPLETE=2};
CsSession *cs_create(const CsConfig *);
/* 仅记录外部后端已发出的本任务请求；此函数没有设备操作。 */
int cs_request_issued(CsSession *,uint64_t task,uint64_t monotonic_ms);
int cs_receive(CsSession *,const X2dExposureInfo *,const X2dCaptureIdentity *,int preview_complete);
/* 每次最多合成一批，不在 owner 线程等待消费者；时间由外部单调时钟提供。 */
int cs_step(CsSession *,uint64_t monotonic_ms);
int cs_cancel(CsSession *);
/* 接收回复的前置条件：该帧已有经验证的真实强引用，与整组消费完成分开。 */
int cs_source_retained(CsSession *,unsigned index,uint32_t exposure_id);
/* 六帧借用与首帧预览副本均已消费；与 RAW/JPEG 的最终保存完成分开。
 * 仅供 owner 线程归还原厂源图像；接收回复已独立完成，不表示原片写盘。 */
int cs_sources_consumed(CsSession *);
const char *cs_failure_stage(CsSession *);
uint64_t cs_compute_ms(CsSession *);
uint64_t cs_input_copy_ms(CsSession *);
uint64_t cs_cached_compute_ms(CsSession *);
/* 活跃消费者未真实返回时拒绝销毁；取消后须继续 pump 到 CS_FAILED。 */
int cs_destroy(CsSession *);
#endif
