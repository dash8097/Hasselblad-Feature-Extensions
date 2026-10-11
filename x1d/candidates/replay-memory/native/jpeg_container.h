#ifndef X1D_JPEG_CONTAINER_H
#define X1D_JPEG_CONTAINER_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define XJ_EXPORT __declspec(dllexport)
#else
#define XJ_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 纯内存接口：没有文件、相机、网络或 Qt 状态访问。返回值不是写卡状态。 */
enum XjStatus {
    XJ_OK = 0,
    XJ_ARGUMENT = 1,
    XJ_JPEG = 2,
    XJ_EXIF = 3,
    XJ_UNSUPPORTED = 4,
    XJ_CAPACITY = 5,
    XJ_NEED_OUTPUT = 6,
    XJ_OVERLAP = 7,
    XJ_NEED_INPUT = 8,
    XJ_CODEC = 9,
    XJ_ALLOCATION = 10
};

#define XJ_PREVIEW_WIDTH 1108u
#define XJ_PREVIEW_HEIGHT 830u
#define XJ_PREVIEW_MAX_BYTES (3u * 1024u * 1024u)
#define XJ_PREFIX_MAX_BYTES (4u * 1024u * 1024u)

typedef struct XjInfo {
    uint32_t width, height, orientation;
    uint32_t exif_offset, exif_size, ifd0_next_offset;
    uint32_t thumbnail_offset, thumbnail_size;
    uint32_t eoi_end, icc_bytes, has_unique_id;
    uint8_t unique_id[32];
    uint32_t preview_kind, header_bytes; /* 0=Exif IFD1；1=本项目 APP15 分段预览。 */
} XjInfo;

/* 校验新编码器使用的 baseline/单次三组件扫描封装；不代替熵解码。 */
XJ_EXPORT int xj_inspect(const uint8_t *jpeg, uint32_t size, XjInfo *info);

/* 检查文件头与分段目录。成功不表示主图/文件已完整写入。 */
XJ_EXPORT int xj_preview_info(const uint8_t *prefix, uint32_t size, XjInfo *info);

/* 提取后验证完整预览 JPEG、尺寸和原厂 ICC 一致性；out=NULL 返回所需容量。 */
XJ_EXPORT int xj_extract_preview(const uint8_t *prefix, uint32_t size,
                                 uint8_t *out, uint32_t capacity, uint32_t *out_size);

/* 只取 TIFF32 头中的 Exif ImageUniqueID；不要求 RAW 像素区位于前缀内。 */
XJ_EXPORT int xj_tiff_unique_id(const uint8_t *prefix, uint32_t size, uint8_t out[32]);

/*
 * 仅接受固定 1.25.0 的 Full 尺寸与 4096 字节 TIFF 容器。
 * preview 必须是已经成功编码的 1108x830 JPEG，不带 Exif/ICC。
 * 原主图的全部 ICC APP2 段会复制进内嵌 JPEG；像素不旋转。
 * 预览分段保存在 APP15；原 Exif 原样保留，方向使用 IFD0。
 * out=NULL 时只返回所需长度；不接受输入/输出缓冲区重叠。
 */
XJ_EXPORT int xj_embed_preview(const uint8_t *jpeg, uint32_t size,
                     const uint8_t *preview, uint32_t preview_size,
                     uint8_t *out, uint32_t capacity, uint32_t *out_size);

#ifdef __cplusplus
}
#endif
#endif
