#ifndef RAW_STACK_ASYNC_H
#define RAW_STACK_ASYNC_H
#include "raw_stack_parallel.h"
struct RsAsyncPool;
/* 四工人和一份源帧快照复用。source_read 后可归还原厂 RAW；
 * 快照保留到 collect，累计区同一时刻只有一个写任务。 */
typedef struct {
 RsCore *core; const uint16_t *source; size_t bytes;
 struct RsAsyncPool *pool; unsigned active,done,source_read; int ok;
 uint64_t started_ns,source_read_ns,done_ns;
} RsAsync;
typedef struct {uint64_t copy_ms,total_ms;unsigned copied,finished;} RsAsyncTiming;
int rs_async_timing(const RsAsync *,RsAsyncTiming *);
int rs_async_prepare(RsAsync *,const RsCore *);
int rs_async_source_read(const RsAsync *);
/* 活跃任务须先 collect；shutdown 不取消正在读图的工人。 */
int rs_async_shutdown(RsAsync *);
int rs_async_start(RsAsync *,RsCore *,const uint16_t *,size_t);
/* -1 无任务，0 尚在运行，1 所有工人已结束本帧；ok 返回计算结果。 */
int rs_async_collect(RsAsync *,int *ok);
#endif
