#include "jpeg_container.h"

#define MAX_FILE (256u * 1024u * 1024u)
#define MAX_ICC 16u
#define ORIGINAL_TIFF 4096u
#define IFD1_BYTES (2u + 7u * 12u + 4u + 16u)
#define MAX_PREVIEW_PARTS 64u
#define PREVIEW_PART_BYTES 65513u
static const uint8_t preview_magic[8] = {'X','1','D','P','V','0','1',0};

typedef struct Segment { uint32_t at, size; } Segment;
typedef struct Parsed {
    XjInfo info;
    uint32_t app1_at, app1_size, icc_count;
    Segment icc[MAX_ICC];
    uint32_t preview_count, preview_expected, preview_total, preview_written;
    Segment preview[MAX_PREVIEW_PARTS];
} Parsed;

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint32_t n)
{ for (uint32_t i = 0; i < n; ++i) dst[i] = src[i]; }
static void zero_bytes(void *dst, uint32_t n)
{ uint8_t *d = (uint8_t *)dst; for (uint32_t i = 0; i < n; ++i) d[i] = 0; }
static int equal_bytes(const uint8_t *a, const uint8_t *b, uint32_t n)
{ for (uint32_t i = 0; i < n; ++i) if (a[i] != b[i]) return 0; return 1; }
static uint32_t be16(const uint8_t *p)
{ return ((uint32_t)p[0] << 8) | p[1]; }
static uint32_t get16(const uint8_t *p, int le)
{ return le ? ((uint32_t)p[1] << 8) | p[0] : be16(p); }
static uint32_t get32(const uint8_t *p, int le)
{
    return le ? ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
                ((uint32_t)p[1] << 8) | p[0] :
                ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                ((uint32_t)p[2] << 8) | p[3];
}
static void put16(uint8_t *p, uint32_t v, int le)
{ p[le ? 0 : 1] = (uint8_t)v; p[le ? 1 : 0] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v, int le)
{ for (uint32_t i = 0; i < 4; ++i) p[le ? i : 3 - i] = (uint8_t)(v >> (i * 8)); }
static int fits(uint32_t at, uint32_t n, uint32_t size)
{ return at <= size && n <= size - at; }

static int ifd_bounds(const uint8_t *tiff, uint32_t size, uint32_t off,
                      int le, uint32_t *count, uint32_t *next_at)
{
    static const uint8_t widths[13] = {0,1,1,2,4,8,1,1,2,4,8,4,8};
    if (off < 8 || (off & 1) || !fits(off, 2, size)) return 0;
    *count = get16(tiff + off, le);
    if (*count > 128 || !fits(off + 2, *count * 12 + 4, size)) return 0;
    *next_at = off + 2 + *count * 12;
    uint32_t last_tag = 0;
    for (uint32_t i = 0; i < *count; ++i) {
        const uint8_t *entry = tiff + off + 2 + i * 12;
        uint32_t tag = get16(entry, le), type = get16(entry + 2, le);
        uint32_t n = get32(entry + 4, le);
        if (type == 0 || type > 12 || n == 0 || (i && tag <= last_tag)) return 0;
        last_tag = tag;
        uint64_t bytes = (uint64_t)n * widths[type];
        if (bytes > size) return 0;
        if (bytes > 4) {
            uint32_t at = get32(entry + 8, le);
            if (at < 8 || !fits(at, (uint32_t)bytes, size)) return 0;
            if (at < *next_at + 4 && at + bytes > off) return 0;
        }
    }
    return 1;
}

static const uint8_t *find_entry(const uint8_t *tiff, uint32_t off,
                                  uint32_t count, uint32_t wanted, int le)
{
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *entry = tiff + off + 2 + i * 12;
        if (get16(entry, le) == wanted) return entry;
    }
    return NULL;
}

int xj_tiff_unique_id(const uint8_t *tiff, uint32_t size, uint8_t out[32])
{
    if (!out) return XJ_ARGUMENT;
    zero_bytes(out, 32);
    if (!tiff || size < 8 || size > 131072) return XJ_ARGUMENT;
    if (!((tiff[0] == 'I' && tiff[1] == 'I') || (tiff[0] == 'M' && tiff[1] == 'M')))
        return XJ_UNSUPPORTED;
    int le = tiff[0] == 'I';
    if (get16(tiff + 2, le) != 42) return XJ_UNSUPPORTED;
    uint32_t off = get32(tiff + 4, le);
    for (uint32_t level = 0; level < 2; ++level) {
        if (off < 8 || (off & 1)) return XJ_EXIF;
        if (!fits(off, 2, size)) return XJ_NEED_INPUT;
        uint32_t count = get16(tiff + off, le);
        if (count > 256) return XJ_EXIF;
        if (!fits(off + 2, count * 12 + 4, size)) return XJ_NEED_INPUT;
        const uint8_t *entry = NULL;
        uint32_t previous = 0, wanted = level ? 0xa420 : 0x8769;
        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t *item = tiff + off + 2 + i * 12;
            uint32_t tag = get16(item, le);
            if (i && tag <= previous) return XJ_EXIF;
            previous = tag;
            if (tag == wanted) entry = item;
        }
        if (!entry) return XJ_UNSUPPORTED;
        if (!level) {
            if (get16(entry + 2, le) != 4 || get32(entry + 4, le) != 1) return XJ_EXIF;
            uint32_t child = get32(entry + 8, le);
            if (child >= off && child < off + 2 + count * 12 + 4) return XJ_EXIF;
            off = child;
            continue;
        }
        if (get16(entry + 2, le) != 2 || get32(entry + 4, le) != 33) return XJ_EXIF;
        uint32_t at = get32(entry + 8, le), nonzero = 0;
        if (!fits(at, 33, size)) return XJ_NEED_INPUT;
        if (at < 8 || (at < off + 2 + count * 12 + 4 && at + 33 > off) || tiff[at + 32])
            return XJ_EXIF;
        uint8_t normalized[32];
        for (uint32_t i = 0; i < 32; ++i) {
            uint8_t c = tiff[at + i];
            if (c >= 'A' && c <= 'F') c = (uint8_t)(c + ('a' - 'A'));
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return XJ_UNSUPPORTED;
            normalized[i] = c;
            if (c != '0') nonzero = 1;
        }
        if (!nonzero) return XJ_UNSUPPORTED;
        copy_bytes(out, normalized, 32);
        return XJ_OK;
    }
    return XJ_UNSUPPORTED;
}

static int read_exif(const uint8_t *jpeg, Parsed *parsed)
{
    XjInfo *info = &parsed->info;
    const uint8_t *tiff = jpeg + info->exif_offset;
    uint32_t size = info->exif_size, n0, next0;
    if (size < 8 || !((tiff[0] == 'I' && tiff[1] == 'I') ||
                      (tiff[0] == 'M' && tiff[1] == 'M'))) return XJ_EXIF;
    int le = tiff[0] == 'I';
    if (get16(tiff + 2, le) != 42) return XJ_EXIF;
    uint32_t off0 = get32(tiff + 4, le);
    if (!ifd_bounds(tiff, size, off0, le, &n0, &next0)) return XJ_EXIF;
    info->ifd0_next_offset = info->exif_offset + next0;
    const uint8_t *entry = find_entry(tiff, off0, n0, 0x112, le);
    if (entry) {
        if (get16(entry + 2, le) != 3 || get32(entry + 4, le) != 1) return XJ_EXIF;
        info->orientation = get16(entry + 8, le);
        if (info->orientation < 1 || info->orientation > 8) return XJ_EXIF;
    }
    entry = find_entry(tiff, off0, n0, 0x8769, le);
    if (entry) {
        if (get16(entry + 2, le) != 4 || get32(entry + 4, le) != 1) return XJ_EXIF;
        uint32_t off = get32(entry + 8, le), count, next;
        if (!ifd_bounds(tiff, size, off, le, &count, &next) ||
            (off < next0 + 4 && next + 4 > off0) || get32(tiff + next, le)) return XJ_EXIF;
        entry = find_entry(tiff, off, count, 0xa420, le);
        if (entry) {
            if (get16(entry + 2, le) != 2 || get32(entry + 4, le) != 33) return XJ_EXIF;
            uint32_t at = get32(entry + 8, le);
            if (!fits(at, 33, size) || tiff[at + 32] != 0) return XJ_EXIF;
            uint32_t nonzero = 0, valid = 1;
            for (uint32_t i = 0; i < 32; ++i) {
                uint8_t c = tiff[at + i];
                if (c >= 'A' && c <= 'F') c = (uint8_t)(c + ('a' - 'A'));
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) valid = 0;
                if (c != '0') nonzero = 1;
                info->unique_id[i] = c;
            }
            info->has_unique_id = valid && nonzero;
            if (!info->has_unique_id) zero_bytes(info->unique_id, 32);
        }
    }
    uint32_t off1 = get32(tiff + next0, le);
    if (off1) {
        if (parsed->preview_count) return XJ_UNSUPPORTED;
        uint32_t count, next;
        if (!ifd_bounds(tiff, size, off1, le, &count, &next) ||
            (off1 < next0 + 4 && next + 4 > off0) || get32(tiff + next, le)) return XJ_EXIF;
        const uint8_t *compression = find_entry(tiff, off1, count, 0x103, le);
        const uint8_t *start = find_entry(tiff, off1, count, 0x201, le);
        const uint8_t *length = find_entry(tiff, off1, count, 0x202, le);
        if (!compression || !start || !length ||
            get16(compression + 2, le) != 3 || get32(compression + 4, le) != 1 ||
            get16(compression + 8, le) != 6 ||
            get16(start + 2, le) != 4 || get32(start + 4, le) != 1 ||
            get16(length + 2, le) != 4 || get32(length + 4, le) != 1) return XJ_EXIF;
        uint32_t at = get32(start + 8, le), len = get32(length + 8, le);
        if (len < 4 || !fits(at, len, size) ||
            (at < next + 4 && at + len > off1) ||
            (at < next0 + 4 && at + len > off0)) return XJ_EXIF;
        info->thumbnail_offset = info->exif_offset + at;
        info->thumbnail_size = len;
    }
    return XJ_OK;
}

static int parse(const uint8_t *jpeg, uint32_t size, Parsed *parsed, int prefix)
{
    static const uint8_t exif[6] = {'E','x','i','f',0,0};
    static const uint8_t icc[12] = {'I','C','C','_','P','R','O','F','I','L','E',0};
    if (!jpeg || !parsed || size < 4 || size > MAX_FILE) return XJ_ARGUMENT;
    zero_bytes(parsed, sizeof(*parsed));
    parsed->info.orientation = 1;
    if (jpeg[0] != 255 || jpeg[1] != 0xd8) return XJ_JPEG;
    uint32_t at = 2, sof = 0, expected_icc = 0;
    for (uint32_t markers = 0; markers < 256 && fits(at, 2, size); ++markers) {
        uint32_t marker_at = at;
        if (jpeg[at++] != 255) return XJ_JPEG;
        while (at < size && jpeg[at] == 255) ++at;
        if (at == size) return prefix ? XJ_NEED_INPUT : XJ_JPEG;
        uint32_t marker = jpeg[at++];
        if (marker == 0 || marker == 1 || (marker >= 0xd0 && marker <= 0xd9)) return XJ_JPEG;
        if (!fits(at, 2, size)) return prefix ? XJ_NEED_INPUT : XJ_JPEG;
        uint32_t length = be16(jpeg + at);
        if (length < 2) return XJ_JPEG;
        if (!fits(at, length, size)) return prefix ? XJ_NEED_INPUT : XJ_JPEG;
        const uint8_t *data = jpeg + at + 2;
        uint32_t bytes = length - 2;
        if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4) {
            if (marker != 0xc0 || sof++ || bytes != 15 || data[0] != 8 || data[5] != 3) return XJ_UNSUPPORTED;
            parsed->info.width = be16(data + 3);
            parsed->info.height = be16(data + 1);
            if (!parsed->info.width || !parsed->info.height ||
                parsed->info.width > 8192 || parsed->info.height > 8192) return XJ_UNSUPPORTED;
        }
        if (marker == 0xe1 && bytes >= 6 && equal_bytes(data, exif, 6)) {
            if (parsed->app1_size) return XJ_EXIF;
            parsed->app1_at = marker_at;
            parsed->app1_size = at + length - marker_at;
            parsed->info.exif_offset = at + 8;
            parsed->info.exif_size = bytes - 6;
        }
        if (marker == 0xe2 && bytes >= 12 && equal_bytes(data, icc, 12)) {
            if (bytes < 14 || parsed->icc_count == MAX_ICC ||
                data[12] != parsed->icc_count + 1 || !data[13] || data[13] > MAX_ICC ||
                (expected_icc && expected_icc != data[13])) return XJ_UNSUPPORTED;
            expected_icc = data[13];
            Segment *segment = &parsed->icc[parsed->icc_count++];
            segment->at = marker_at;
            segment->size = at + length - marker_at;
            parsed->info.icc_bytes += segment->size;
            if (parsed->info.icc_bytes > 65533) return XJ_CAPACITY;
        }
        if (marker == 0xef && bytes >= 8 && equal_bytes(data, preview_magic, 8)) {
            if (bytes < 21) return XJ_JPEG;
            uint32_t total = get32(data + 8, 0), index = get16(data + 12, 0);
            uint32_t count = get16(data + 14, 0), offset = get32(data + 16, 0), chunk = bytes - 20;
            if (!total || total > XJ_PREVIEW_MAX_BYTES || !count || count > MAX_PREVIEW_PARTS ||
                index != parsed->preview_count || index >= count ||
                offset != parsed->preview_written || offset > total || chunk > total - offset ||
                (index + 1 < count && chunk != PREVIEW_PART_BYTES) ||
                (parsed->preview_expected && (parsed->preview_expected != count || parsed->preview_total != total)))
                return XJ_JPEG;
            parsed->preview_total = total;
            parsed->preview_expected = count;
            parsed->preview[parsed->preview_count].at = at + 22;
            parsed->preview[parsed->preview_count++].size = chunk;
            parsed->preview_written += chunk;
        }
        at += length;
        if (marker != 0xda) continue;
        if (sof != 1 || bytes != 10 || data[0] != 3 || data[7] || data[8] != 63 || data[9] ||
            parsed->icc_count != expected_icc) return XJ_UNSUPPORTED;
        parsed->info.header_bytes = at;
        if (parsed->preview_count) {
            if (parsed->preview_count != parsed->preview_expected || parsed->preview_written != parsed->preview_total)
                return XJ_JPEG;
            parsed->info.preview_kind = 1;
            parsed->info.thumbnail_offset = parsed->preview[0].at;
            parsed->info.thumbnail_size = parsed->preview_total;
        }
        if (prefix) return parsed->app1_size ? read_exif(jpeg, parsed) : XJ_UNSUPPORTED;
        /* 单扫描 baseline：FF00 是数据转义，RST 是扫描内部标记。 */
        while (at < size) {
            if (jpeg[at++] != 255) continue;
            while (at < size && jpeg[at] == 255) ++at;
            if (at == size) return XJ_JPEG;
            uint32_t code = jpeg[at++];
            if (code == 0 || (code >= 0xd0 && code <= 0xd7)) continue;
            if (code != 0xd9) return XJ_JPEG;
            parsed->info.eoi_end = at;
            if (size - at > 511) return XJ_JPEG;
            for (; at < size; ++at) if (jpeg[at]) return XJ_JPEG;
            return parsed->app1_size ? read_exif(jpeg, parsed) : XJ_OK;
        }
        return XJ_JPEG;
    }
    return prefix ? XJ_NEED_INPUT : XJ_JPEG;
}

int xj_inspect(const uint8_t *jpeg, uint32_t size, XjInfo *info)
{
    if (!info) return XJ_ARGUMENT;
    Parsed parsed;
    int result = parse(jpeg, size, &parsed, 0);
    zero_bytes(info, sizeof(*info));
    if (result == XJ_OK) copy_bytes((uint8_t *)info, (const uint8_t *)&parsed.info, sizeof(*info));
    return result;
}

int xj_preview_info(const uint8_t *prefix, uint32_t size, XjInfo *info)
{
    if (!info) return XJ_ARGUMENT;
    zero_bytes(info, sizeof(*info));
    if (size > XJ_PREFIX_MAX_BYTES) return XJ_ARGUMENT;
    Parsed main, thumbnail;
    int result = parse(prefix, size, &main, 1);
    if (result != XJ_OK) return result;
    if (main.info.width != 8176 || main.info.height != 6128 || !main.info.thumbnail_size)
        return XJ_UNSUPPORTED;
    if (main.info.preview_kind == 1) {
        copy_bytes((uint8_t *)info, (const uint8_t *)&main.info, sizeof(*info));
        return XJ_OK;
    }
    const uint8_t *small = prefix + main.info.thumbnail_offset;
    result = parse(small, main.info.thumbnail_size, &thumbnail, 0);
    if (result != XJ_OK) return result;
    if (thumbnail.info.width != 1108 || thumbnail.info.height != 830 ||
        main.icc_count != thumbnail.icc_count) return XJ_UNSUPPORTED;
    for (uint32_t i = 0; i < main.icc_count; ++i) {
        if (main.icc[i].size != thumbnail.icc[i].size ||
            !equal_bytes(prefix + main.icc[i].at, small + thumbnail.icc[i].at, main.icc[i].size))
            return XJ_UNSUPPORTED;
    }
    copy_bytes((uint8_t *)info, (const uint8_t *)&main.info, sizeof(*info));
    return XJ_OK;
}

static int overlaps(const uint8_t *a, uint32_t an, const uint8_t *b, uint32_t bn)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x <= y ? y - x < an : x - y < bn;
}

int xj_extract_preview(const uint8_t *prefix, uint32_t size,
                         uint8_t *out, uint32_t capacity, uint32_t *out_size)
{
    if (!out_size) return XJ_ARGUMENT;
    *out_size = 0;
    if (size > XJ_PREFIX_MAX_BYTES) return XJ_ARGUMENT;
    Parsed main, thumbnail;
    int status = parse(prefix, size, &main, 1);
    if (status != XJ_OK) return status;
    if (main.info.width != 8176 || main.info.height != 6128 || !main.info.thumbnail_size)
        return XJ_UNSUPPORTED;
    uint32_t bytes = main.info.thumbnail_size;
    if (bytes > XJ_PREVIEW_MAX_BYTES) return XJ_CAPACITY;
    if (!out) { *out_size = bytes; return XJ_NEED_OUTPUT; }
    if (capacity < bytes) return XJ_CAPACITY;
    if (overlaps(out, bytes, prefix, size)) return XJ_OVERLAP;
    if (main.info.preview_kind == 1) {
        uint32_t offset = 0;
        for (uint32_t i = 0; i < main.preview_count; ++i) {
            copy_bytes(out + offset, prefix + main.preview[i].at, main.preview[i].size);
            offset += main.preview[i].size;
        }
    } else copy_bytes(out, prefix + main.info.thumbnail_offset, bytes);
    status = parse(out, bytes, &thumbnail, 0);
    if (status != XJ_OK) return status;
    if (thumbnail.info.width != XJ_PREVIEW_WIDTH || thumbnail.info.height != XJ_PREVIEW_HEIGHT ||
        thumbnail.info.exif_size || thumbnail.preview_count || main.icc_count != thumbnail.icc_count)
        return XJ_UNSUPPORTED;
    for (uint32_t i = 0; i < main.icc_count; ++i)
        if (main.icc[i].size != thumbnail.icc[i].size ||
            !equal_bytes(prefix + main.icc[i].at, out + thumbnail.icc[i].at, main.icc[i].size))
            return XJ_UNSUPPORTED;
    *out_size = bytes;
    return XJ_OK;
}

int xj_embed_preview(const uint8_t *jpeg, uint32_t size,
                     const uint8_t *preview, uint32_t preview_size,
                     uint8_t *out, uint32_t capacity, uint32_t *out_size)
{
    if (!out_size) return XJ_ARGUMENT;
    *out_size = 0;
    Parsed source, small;
    int result = parse(jpeg, size, &source, 0);
    if (result != XJ_OK) return result;
    result = parse(preview, preview_size, &small, 0);
    if (result != XJ_OK) return result;
    if (source.info.width != 8176 || source.info.height != 6128 ||
        source.info.exif_size != ORIGINAL_TIFF || source.info.thumbnail_size ||
        small.info.width != XJ_PREVIEW_WIDTH || small.info.height != XJ_PREVIEW_HEIGHT ||
        small.app1_size || small.icc_count || small.preview_count) return XJ_UNSUPPORTED;
    preview_size = small.info.eoi_end;
    uint32_t thumb_size = preview_size + source.info.icc_bytes;
    if (thumb_size > XJ_PREVIEW_MAX_BYTES) return XJ_CAPACITY;
    uint32_t parts = (thumb_size + PREVIEW_PART_BYTES - 1) / PREVIEW_PART_BYTES;
    if (!parts || parts > MAX_PREVIEW_PARTS) return XJ_CAPACITY;
    uint32_t total = size + thumb_size + parts * 24;
    if (total > MAX_FILE) return XJ_CAPACITY;
    *out_size = total;
    if (!out) return XJ_NEED_OUTPUT;
    if (capacity < total) return XJ_CAPACITY;
    if (overlaps(out, total, jpeg, size) || overlaps(out, total, preview, preview_size)) return XJ_OVERLAP;
    typedef struct Input { const uint8_t *data; uint32_t size; } Input;
    Input inputs[MAX_ICC + 2];
    uint32_t input_count = 0;
    inputs[input_count++] = (Input){preview, 2};
    for (uint32_t i = 0; i < source.icc_count; ++i)
        inputs[input_count++] = (Input){jpeg + source.icc[i].at, source.icc[i].size};
    inputs[input_count++] = (Input){preview + 2, preview_size - 2};
    out[0] = 255; out[1] = 0xd8;
    uint32_t at = 2, offset = 0, source_index = 0, source_offset = 0;
    for (uint32_t part = 0; part < parts; ++part) {
        uint32_t chunk = thumb_size - offset;
        if (chunk > PREVIEW_PART_BYTES) chunk = PREVIEW_PART_BYTES;
        out[at] = 255; out[at + 1] = 0xef;
        put16(out + at + 2, chunk + 22, 0);
        copy_bytes(out + at + 4, preview_magic, 8);
        put32(out + at + 12, thumb_size, 0);
        put16(out + at + 16, part, 0); put16(out + at + 18, parts, 0);
        put32(out + at + 20, offset, 0);
        uint32_t copied = 0;
        while (copied < chunk) {
            uint32_t n = inputs[source_index].size - source_offset;
            if (n > chunk - copied) n = chunk - copied;
            copy_bytes(out + at + 24 + copied, inputs[source_index].data + source_offset, n);
            copied += n; source_offset += n;
            if (source_offset == inputs[source_index].size) { ++source_index; source_offset = 0; }
        }
        at += chunk + 24;
        offset += chunk;
    }
    copy_bytes(out + at, jpeg + 2, size - 2);
    return XJ_OK;
}
