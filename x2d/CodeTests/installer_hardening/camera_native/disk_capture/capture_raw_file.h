/* 顺序 3FR 文件消费者；不会登记相册、重启服务或读取原片。 */
#ifndef CAPTURE_RAW_FILE_H
#define CAPTURE_RAW_FILE_H
#include "capture_raw_sink.h"
typedef struct CrFile CrFile;
/* 只创建不存在的路径；source/merged 是已经持有的内存头部，preview 被复制。
 * 失败只删除仍属于本次独占创建的半成品，不修改用户已有文件。 */
CrFile *cr_file_open(const char *path,uint64_t task,const uint8_t source[F3_HEADER],
 const uint8_t merged[4096],const uint8_t *preview,size_t preview_bytes);
CrFile *cr_file_open_diagnostic(const char *,uint64_t,const uint8_t *,const uint8_t *,const uint8_t *,size_t,const char **);
int cr_file_chunk(CrFile *,const PcChunk *);
int cr_file_finish(CrFile *);
/* finish 成功保留完整私有成片；其余状态安全关闭并清理本次半成品。 */
void cr_file_destroy(CrFile *);
#endif
