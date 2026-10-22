#ifndef X2D_PRECAPTURE_FRAME420_H
#define X2D_PRECAPTURE_FRAME420_H
#include "precapture_ring.h"
/* 只解析已核验的4.2.0 duss_frame_buffer描述，不能传任意cam_frame_buffer包装对象。
 * mapped 必须由适配层完成CPU映射和DMA同步，mapped_bytes必须是真实映射边界。
 * 本函数不映射句柄、不获取/释放原厂引用、不猜测时钟；时间/序号/epoch由适配层核对。
 * 输出只在同步pc_push_nv12返回前有效，不可带指针排队给异步工作线程。 */
int pc_frame420_nv12(const void *descriptor,size_t descriptor_bytes,
    const void *mapped,size_t mapped_bytes,uint64_t timestamp_ns,uint64_t sequence,
    uint64_t stream_epoch,struct pc_frame *out);
#endif
