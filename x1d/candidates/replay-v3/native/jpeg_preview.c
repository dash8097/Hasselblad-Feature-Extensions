#include "jpeg_preview.h"

/* 此除法仅用于 <=2044 像素的坐标步长，避免给纯 C 核心引入运行库依赖。 */
static uint32_t quotient(uint32_t n, uint32_t d)
{
    uint32_t value = 0, remainder = 0;
    for (int bit = 31; bit >= 0; --bit) {
        remainder = (remainder << 1) | ((n >> bit) & 1);
        if (remainder >= d) { remainder -= d; value |= (uint32_t)1 << bit; }
    }
    return value;
}

static void resize_rgb(const uint8_t *src, uint32_t sw, uint32_t sh,
                       uint8_t *dst, uint32_t dw, uint32_t dh)
{
    uint32_t dx = quotient((sw - 1) << 16, dw - 1);
    uint32_t dy = quotient((sh - 1) << 16, dh - 1);
    for (uint32_t y = 0, py = 0; y < dh; ++y, py += dy) {
        uint32_t sy = py >> 16, wy = (py >> 8) & 255;
        if (sy + 1 >= sh) { sy = sh - 2; wy = 256; }
        for (uint32_t x = 0, px = 0; x < dw; ++x, px += dx) {
            uint32_t sx = px >> 16, wx = (px >> 8) & 255;
            if (sx + 1 >= sw) { sx = sw - 2; wx = 256; }
            const uint8_t *a = src + (sy * sw + sx) * 3;
            const uint8_t *b = a + sw * 3;
            for (uint32_t c = 0; c < 3; ++c) {
                uint32_t upper = a[c] * (256 - wx) + a[c + 3] * wx;
                uint32_t lower = b[c] * (256 - wx) + b[c + 3] * wx;
                *dst++ = (uint8_t)((upper * (256 - wy) + lower * wy + 32768) >> 16);
            }
        }
    }
}

int xj_make_preview(const uint8_t *jpeg, uint32_t size,
                     uint8_t *out, uint32_t capacity,
                     XjPreviewResult *result, const XjCodec *codec)
{
    if (!result) return XJ_ARGUMENT;
    uint8_t *clear = (uint8_t *)result;
    for (uint32_t i = 0; i < sizeof(*result); ++i) clear[i] = 0;
    if (!jpeg || !out || !codec || codec->size != sizeof(*codec) || codec->version != 1 ||
        !codec->init_decompress || !codec->init_compress || !codec->destroy || !codec->allocate ||
        !codec->release || !codec->header || !codec->scaling || !codec->decompress ||
        !codec->buffer_size || !codec->compress) return XJ_ARGUMENT;
    if (((uintptr_t)out <= (uintptr_t)jpeg && (uintptr_t)jpeg - (uintptr_t)out < capacity) ||
        ((uintptr_t)out > (uintptr_t)jpeg && (uintptr_t)out - (uintptr_t)jpeg < size)) return XJ_OVERLAP;
    XjInfo info;
    int status = xj_inspect(jpeg, size, &info);
    if (status != XJ_OK) return status;
    if (info.width != 8176 || info.height != 6128)
        return XJ_UNSUPPORTED;
    uint32_t budget = 0;
    void *decoder = NULL, *encoder = NULL;
    uint8_t *decoded = NULL, *resized = NULL, *encoded = NULL;
    decoder = codec->init_decompress();
    if (!decoder) return XJ_ALLOCATION;
    int width, height, sampling, colorspace, count = 0;
    status = XJ_CODEC;
    if (codec->header(decoder, (uint8_t *)jpeg, size, &width, &height, &sampling, &colorspace)) goto done;
    if (width != 8176 || height != 6128 || (colorspace != 0 && colorspace != 1)) goto done;
    XjScalingFactor *factors = codec->scaling(&count);
    if (!factors || count < 1 || count > 32) goto done;
    int supported = 0;
    for (int i = 0; i < count; ++i) if (factors[i].num == 1 && factors[i].denom == 4) supported = 1;
    if (!supported) goto done;
    uint32_t rgb_bytes = 2044 * 1532 * 3, resize_bytes = 1108 * 830 * 3;
    decoded = codec->allocate((int)rgb_bytes);
    status = XJ_ALLOCATION;
    if (!decoded) goto done;
    status = XJ_CODEC;
    /* TJPF_RGB=0、TJFLAG_ACCURATEDCT=4096。 */
    if (codec->decompress(decoder, (uint8_t *)jpeg, size, decoded, 2044, 2044 * 3, 1532, 0, 4096)) goto done;
    result->main_decoded = 1;
    result->decoded_rgb_bytes = rgb_bytes;
    codec->destroy(decoder); decoder = NULL;
    /* 从这里起的失败只影响可选预览，主图已有解码依据。 */
    status = XJ_UNSUPPORTED;
    if (info.exif_size != 4096 || info.thumbnail_size) goto done;
    status = XJ_CAPACITY;
    budget = XJ_PREVIEW_MAX_BYTES;
    if (info.icc_bytes >= budget) goto done;
    budget -= info.icc_bytes;
    if (capacity < budget) budget = capacity;
    if (budget < 4096) goto done;
    status = XJ_ALLOCATION;
    resized = codec->allocate((int)resize_bytes);
    if (!resized) goto done;
    encoder = codec->init_compress();
    status = XJ_ALLOCATION;
    if (!encoder) goto done;
    unsigned long encoded_capacity = codec->buffer_size(1108, 830, 2); /* TJSAMP_420 */
    if (encoded_capacity < 1024 || encoded_capacity > 4 * 1024 * 1024) { status = XJ_CODEC; goto done; }
    encoded = codec->allocate((int)encoded_capacity);
    if (!encoded) goto done;
    static const uint8_t qualities[] = {85};
    result->decoded_rgb_bytes = rgb_bytes;
    result->scratch_bytes = rgb_bytes + resize_bytes + (uint32_t)encoded_capacity;
    {
        uint32_t w = 1108, h = 830;
        resize_rgb(decoded, 2044, 1532, resized, w, h);
        for (uint32_t q = 0; q < sizeof(qualities); ++q) {
            unsigned long bytes = encoded_capacity;
            uint8_t *buffer = encoded;
            ++result->attempts;
            /* NOREALLOC；分配上限来自 tjBufSize，不让压缩器扩大缓冲。 */
            if (codec->compress(encoder, resized, (int)w, (int)(w * 3), (int)h, 0,
                                 &buffer, &bytes, 2, qualities[q], 1024 | 4096) ||
                buffer != encoded || bytes > encoded_capacity) { status = XJ_CODEC; goto done; }
            if (bytes > budget) continue;
            XjInfo preview;
            if (xj_inspect(encoded, (uint32_t)bytes, &preview) != XJ_OK ||
                preview.width != w || preview.height != h || preview.exif_size || preview.icc_bytes) {
                status = XJ_CODEC; goto done;
            }
            for (uint32_t i = 0; i < bytes; ++i) out[i] = encoded[i];
            result->bytes = (uint32_t)bytes;
            result->width = w; result->height = h; result->quality = qualities[q];
            status = XJ_OK;
            goto done;
        }
    }
    status = XJ_CAPACITY;
done:
    if (encoded) codec->release(encoded);
    if (resized) codec->release(resized);
    if (decoded) codec->release(decoded);
    if (encoder) codec->destroy(encoder);
    if (decoder) codec->destroy(decoder);
    return status;
}
