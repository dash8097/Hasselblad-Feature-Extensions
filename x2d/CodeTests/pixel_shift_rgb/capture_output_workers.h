/* 两条长期消费者线程；不自行调用原厂 QObject 或回收合成缓冲。
 * 控制线程取出完成通知后，才调用 cmp_done 归还对应租约。 */
#ifndef CAPTURE_OUTPUT_WORKERS_H
#define CAPTURE_OUTPUT_WORKERS_H
#include "pixel_chunk_fanout.h"
typedef struct CwWorkers CwWorkers;
typedef struct {int (*consume)(void *,const PcChunk *);void *context;} CwConsumer;
typedef struct {unsigned consumer;uint64_t task,ticket;int ok;} CwCompletion;
CwWorkers *cw_create(uint64_t task,const CwConsumer consumers[PC_CONSUMERS]);
int cw_submit(CwWorkers *,unsigned consumer,const PcChunk *);
int cw_poll(CwWorkers *,unsigned consumer,CwCompletion *);
/* 等已交出的消费者真实返回。不会强制取消正在使用的原厂/文件缓冲。 */
void cw_join(CwWorkers *);
/* 必须先取回所有完成通知；尚有租约时拒绝销毁。 */
int cw_destroy(CwWorkers *);
#endif
