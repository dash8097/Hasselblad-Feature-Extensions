/* RAW 堆栈计算核心；不连接设备、不写文件。相同 CFA 位置直接累积。 */
#ifndef RAW_STACK_CORE_H
#define RAW_STACK_CORE_H
#include <stddef.h>
#include <stdint.h>
#define RS_MAX_FRAMES UINT64_C(65535)
typedef enum { RS_MAXIMUM=0, RS_MINIMUM=1, RS_AVERAGE=2 } RsMethod;
typedef struct {
 uint32_t width,height,stride_samples;
 RsMethod method;
 /* 最大/最小值先对 noise_group 张求平均；0 表示关闭。 */
 uint32_t noise_group;
} RsConfig;
#define RS_MAX_SEGMENTS 8
typedef struct {void *workspace;size_t bytes;uint32_t rows;} RsSegment;
typedef struct {const uint16_t *pixels;size_t bytes;} RsPixelBlock;
typedef struct {
 RsConfig config;
 void *workspace;
 size_t bytes,samples;
 uint64_t frames;
 uint32_t pending_group;
 unsigned extrema_ready,frame_open,finished,failed;
 RsSegment segments[RS_MAX_SEGMENTS];
 unsigned segment_count;
} RsCore;
int rs_workspace_bytes(RsConfig,size_t *);
int rs_init(RsCore *,RsConfig,void *,size_t);
/* 行段共享帧计数和分组状态；无需一块物理连续的整幅累加区。 */
int rs_init_segments(RsCore *,RsConfig,const RsSegment *,unsigned);
/* 调用者在帧入口核对完整范围/几何，只允许互不重叠的行段并行执行。
 * 每帧所有行段完成后才能 commit；source 在此之前必须保持强引用。 */
int rs_begin_frame(RsCore *,size_t source_bytes);
int rs_accumulate_rows(RsCore *,const uint16_t *,uint32_t first,uint32_t rows);
int rs_accumulate_block(RsCore *,const uint16_t *block,size_t bytes,uint32_t first,uint32_t rows);
int rs_commit_frame(RsCore *);
/* begin_finish 后对所有行段 normalize，再由单个所有者 compact。
 * compact 的原地重排不允许和 normalize 并发。 */
int rs_begin_finish(RsCore *);
int rs_normalize_rows(RsCore *,uint32_t first,uint32_t rows);
const uint16_t *rs_compact(RsCore *,size_t *bytes);
unsigned rs_compact_blocks(RsCore *,RsPixelBlock *,unsigned,size_t *bytes);
#endif
