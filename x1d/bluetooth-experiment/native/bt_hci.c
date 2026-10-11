#include "bt_hci.h"
#include <string.h>

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

int bt_build(enum bt_command command, struct bt_frame *f) {
    static const uint16_t opcodes[] = {
        0x1001, 0x1003, 0x2003, 0x2006, 0x2008,
        0x200a, 0x200a, 0x200b, 0x200c, 0x200c
    };
    static const char name[] = "X1D-BT-Test";
    uint8_t *p;
    unsigned length = 0;
    if (!f || (unsigned)command >= sizeof(opcodes)/sizeof(opcodes[0])) return -1;
    memset(f, 0, sizeof(*f));
    f->opcode = opcodes[command];
    f->bytes[0] = 1;
    f->bytes[1] = (uint8_t)f->opcode;
    f->bytes[2] = (uint8_t)(f->opcode >> 8);
    p = f->bytes + 4;
    switch (command) {
    case BT_ADV_PARAMETERS:
        length = 15;
        p[0] = 0xa0; p[2] = 0xa0; /* 100ms：参数值，不是实测发射间隔。 */
        p[4] = 3; /* ADV_NONCONN_IND：不可连接广播。 */
        p[13] = 7; /* 三个广播信道；公共本机地址，不设置或伪造地址。 */
        break;
    case BT_ADV_DATA:
        length = 32;
        p[0] = (uint8_t)(3 + 2 + sizeof(name) - 1);
        p[1] = 2; p[2] = 1; p[3] = 2; /* Flags: LE General Discoverable。 */
        p[4] = (uint8_t)sizeof(name); p[5] = 9;
        memcpy(p + 6, name, sizeof(name) - 1);
        break;
    case BT_ADV_ENABLE: length = 1; p[0] = 1; break;
    case BT_ADV_DISABLE: length = 1; break;
    case BT_PASSIVE_SCAN_PARAMETERS:
        length = 7;
        p[0] = 0; /* 被动扫描，不发 scan request。 */
        p[1] = 0x10; p[3] = 0x10;
        break;
    case BT_SCAN_ENABLE: length = 2; p[0] = 1; p[1] = 1; break;
    case BT_SCAN_DISABLE: length = 2; break;
    default: break;
    }
    f->bytes[3] = (uint8_t)length;
    f->length = length + 4;
    return 0;
}

void bt_parser_init(struct bt_event_parser *p) { memset(p, 0, sizeof(*p)); }

int bt_feed(struct bt_event_parser *p, uint8_t b, uint16_t expected, struct bt_reply *r) {
    int result;
    if (!p || !r || p->failed) return BT_BAD_FRAME;
    switch (p->stage) {
    case 0:
        /* 不扫描垃圾字节重新同步，未知线路数据立即拒绝。 */
        if (b != 4) { p->failed = 1; return BT_BAD_FRAME; }
        p->stage = 1; return BT_MORE;
    case 1: p->event = b; p->stage = 2; return BT_MORE;
    case 2:
        p->length = b; p->used = 0; p->stage = 3;
        if (!b) { p->failed = 1; return BT_BAD_FRAME; }
        return BT_MORE;
    default:
        p->payload[p->used++] = b;
        if (p->used < p->length) return BT_MORE;
        break;
    }
    memset(r, 0, sizeof(*r));
    if (p->event == 0x0e) {
        if (p->length < 4) { p->failed = 1; return BT_BAD_FRAME; }
        r->credits = p->payload[0]; r->opcode = le16(p->payload + 1);
        r->status = p->payload[3]; r->length = p->length - 4;
        memcpy(r->data, p->payload + 4, r->length);
        result = r->opcode != expected ? BT_UNRELATED :
            (r->status ? BT_CONTROLLER_ERROR : BT_COMPLETE);
    } else if (p->event == 0x0f) {
        if (p->length != 4) { p->failed = 1; return BT_BAD_FRAME; }
        r->status = p->payload[0]; r->credits = p->payload[1];
        r->opcode = le16(p->payload + 2);
        /* 白名单命令要求 Command Complete，Status=0 不能冒充完成。 */
        result = r->opcode != expected ? BT_UNRELATED :
            (r->status ? BT_CONTROLLER_ERROR : BT_UNEXPECTED_STATUS);
    } else {
        result = BT_UNRELATED;
    }
    memset(p->payload, 0, sizeof(p->payload));
    p->stage = 0;
    return result;
}

int bt_validate_reply(enum bt_command cmd, const struct bt_reply *r) {
    struct bt_frame f;
    size_t expected_length;
    if (!r || bt_build(cmd, &f) || r->opcode != f.opcode || r->status) return -1;
    expected_length = (cmd == BT_VERSION || cmd == BT_FEATURES || cmd == BT_LE_FEATURES) ? 8 : 0;
    return r->length == expected_length ? 0 : -1;
}

int bt_count_x2d(const uint8_t *p, size_t n, unsigned *reports, unsigned *names) {
    size_t off = 2;
    unsigned count = 0, matches = 0;
    if (!p || !reports || !names || n < 2 || p[0] != 2 || !p[1]) return -1;
    for (unsigned i = 0; i < p[1]; ++i) {
        size_t data_length, end, a;
        int match = 0;
        if (n - off < 10) return -1;
        /* event_type, address_type, address[6], data_length, data[], RSSI */
        data_length = p[off + 8];
        if (data_length > 31 || data_length + 10 > n - off) return -1;
        a = off + 9; end = a + data_length;
        while (a < end) {
            unsigned field = p[a++];
            if (!field) {
                while (a < end) if (p[a++]) return -1;
                break;
            }
            if (field > end - a) return -1;
            if ((p[a] == 8 || p[a] == 9) && field >= 4 &&
                memcmp(p + a + 1, "X2D", 3) == 0) match = 1;
            a += field;
        }
        off = end + 1;
        count++; matches += (unsigned)match;
    }
    if (off != n) return -1;
    *reports = count; *names = matches;
    return 0;
}
