/* 将协调层接到现有顺序 3FR 写入器；头部须由调用方已验证的内存出口提供。
 * 不从照片重读头部，不发布相册。renderer.consume 是同步工作线程适配，
 * 原厂显影 QObject 的排队和完成等待必须由外部适配实现。 */
#ifndef CAPTURE_SESSION_RAW_OUTPUT_H
#define CAPTURE_SESSION_RAW_OUTPUT_H
#include "capture_memory_session.h"
#include "capture_raw_file.h"
typedef struct {
 int (*begin)(void *,const uint8_t *,size_t,unsigned,unsigned);
 int (*consume)(void *,const PcChunk *);
 int (*finish)(void *);
 void (*abort)(void *);
 void *context;
} CsRawRenderApi;
typedef struct {
 uint64_t task;
 char path[1024];
 uint8_t source[F3_HEADER],merged[4096];
 CsRawRenderApi render;
 CrFile *file;
 const char *begin_error;
 unsigned started,render_started,closed;
} CsRawOutput;
/* 首次调用前必须零初始化；活跃输出拒绝重绑。 */
int cs_raw_output_bind(CsRawOutput *,uint64_t task,const char *path,
 const uint8_t source[F3_HEADER],const uint8_t merged[4096],CsRawRenderApi,CsOutputApi *);
#endif
