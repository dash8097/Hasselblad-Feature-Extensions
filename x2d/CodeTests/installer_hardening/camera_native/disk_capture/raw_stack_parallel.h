#ifndef RAW_STACK_PARALLEL_H
#define RAW_STACK_PARALLEL_H
#include "raw_stack_core.h"
/* 完整源帧保留到线程全部 join。工人只接触连续行块，不调用原厂对象接口。 */
int rs_parallel_frame(RsCore *,const uint16_t *,size_t bytes,unsigned threads);
const uint16_t *rs_parallel_finish(RsCore *,unsigned threads,size_t *bytes);
unsigned rs_parallel_finish_blocks(RsCore *,unsigned,RsPixelBlock *,unsigned,size_t *);
#endif
