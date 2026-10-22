#ifndef CAPTURE_RENDER_STREAM_H
#define CAPTURE_RENDER_STREAM_H
#include "capture_session_raw_output.h"
typedef struct {
 uint64_t task,ticket;
 unsigned next,index,started,completed;
 int fd;
 char socket_path[108];
 const uint8_t *metadata;
 size_t metadata_bytes;
 unsigned full_result,full_started,ion_started;
 void *ion,*result,*result_map;
 int (*ion_stop)(void *,void *),(*ion_close)(void *);
 int (*result_unmap)(void *),(*result_free)(void *),(*result_sync)(void *,uint32_t);
} CsRenderStream;
/* 只允许受限任务目录中的端点，实际连接在 output.begin 执行。
 * 首次使用须零初始化；服务端身份必须是原有显影进程。 */
int cs_render_stream_bind(CsRenderStream *,uint64_t,const char *,CsRawRenderApi *);
/* 数据借用保持到消费者 finish/abort；只允许已认领首帧的真实容器。 */
int cs_render_stream_bind_metadata(CsRenderStream *,uint64_t,const char *,
 const uint8_t *,size_t,CsRawRenderApi *);
int cs_render_stream_bind_full(CsRenderStream *,uint64_t,const char *,
 const uint8_t *,size_t,CsRawRenderApi *);
uint16_t *cs_render_stream_full_pixels(CsRenderStream *,size_t);
#endif
