/* 四个持久计算线程：按源行分段，输出互不重叠，返回前全部完成。
 * 只访问借用的像素，不调用原厂对象，也不释放源引用。 */
#ifndef CAPTURE_MERGE_WORKERS_H
#define CAPTURE_MERGE_WORKERS_H
#include "capture_memory_merge.h"
typedef struct CmwWorkers CmwWorkers;
CmwWorkers *cmw_create(unsigned width);
CmwWorkers *cmw_create_rows(unsigned width,unsigned rows);
size_t cmw_buffer_bytes(unsigned width,unsigned rows);
unsigned cmw_capacity(const CmwWorkers *);
uint64_t cmw_read_ms(const CmwWorkers *);
uint64_t cmw_compute_ms(const CmwWorkers *);
int cmw_run(CmwWorkers *,const CmView[6],unsigned height,unsigned first,unsigned rows,
            uint16_t *output,size_t output_bytes);
void cmw_destroy(CmwWorkers *);
#endif
