#ifndef X1D_JPEG_PREVIEW_H
#define X1D_JPEG_PREVIEW_H
#include "jpeg_container.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XjScalingFactor { int num, denom; } XjScalingFactor;

/* TurboJPEG 1.4 公共 C ABI；指针由原生适配器提供，不保存硬编码库地址。 */
typedef struct XjCodec {
    uint32_t size, version;
    void *(*init_decompress)(void);
    void *(*init_compress)(void);
    int (*destroy)(void *handle);
    uint8_t *(*allocate)(int bytes);
    void (*release)(uint8_t *buffer);
    int (*header)(void *, uint8_t *, unsigned long, int *, int *, int *, int *);
    XjScalingFactor *(*scaling)(int *count);
    int (*decompress)(void *, uint8_t *, unsigned long, uint8_t *, int, int, int, int, int);
    unsigned long (*buffer_size)(int width, int height, int subsampling);
    int (*compress)(void *, uint8_t *, int, int, int, int, uint8_t **,
                    unsigned long *, int, int, int);
} XjCodec;

typedef struct XjPreviewResult {
    uint32_t bytes, width, height, quality, attempts;
    uint32_t decoded_rgb_bytes, scratch_bytes;
    /* 仅在主码流实际解码成功后置 1；预览失败不能替代主图验证。v3 ABI。 */
    uint32_t main_decoded;
} XjPreviewResult;

/* 成片主 JPEG 的 1/4 DCT 缩放后生成 1108x830（919640 像素）预览。 */
XJ_EXPORT int xj_make_preview(const uint8_t *jpeg, uint32_t size,
                              uint8_t *out, uint32_t capacity,
                              XjPreviewResult *result, const XjCodec *codec);

#ifdef __cplusplus
}
#endif
#endif
