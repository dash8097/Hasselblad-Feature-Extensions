/* 固定 4.2.0 的真实拍摄 metadata -> 原厂序列化 DCAM 容器。
 * 只在存储 owner 线程复制本进程内存，不读取参考照片、不记录容器内容。 */
#ifndef CAPTURE_METADATA_NATIVE_H
#define CAPTURE_METADATA_NATIVE_H
#include "capture_header_export.h"
#include "capture_memory_native.h"
#define CH_METADATA_MAX (16u*1024u*1024u)
typedef struct {uint8_t *bytes;size_t size;} ChMetadata;
/* out 必须为空，map 为当前已认领第一帧的真实 QMap 地址。
 * 成功返回独立内存副本；失败不改变 out。缺失/空/超长容器均拒绝。 */
int ch_metadata_copy(CmNative *,ChReadApi,const void *map,ChMetadata *out);
/* map 为第一帧已完成的 FileContent 元数据；失败保持原容器。
 * 与原厂 RAW 回放相同的真实 WB 比例和 neutral gain 转换。 */
int ch_metadata_rawpb_wb(CmNative *,ChReadApi,const void *map,ChMetadata *);
/* 只使用 WriteFileContent 正在持有的完整 Metadata；拒绝文件解析回退。 */
int ch_metadata_write_rawpb(CmNative *,ChReadApi,const void *content,ChMetadata *);
void ch_metadata_destroy(ChMetadata *);
#endif
