#ifndef X1D_BT_HCI_H
#define X1D_BT_HCI_H
#include <stddef.h>
#include <stdint.h>

enum bt_command {
    BT_VERSION, BT_FEATURES, BT_LE_FEATURES,
    BT_ADV_PARAMETERS, BT_ADV_DATA, BT_ADV_ENABLE, BT_ADV_DISABLE,
    BT_PASSIVE_SCAN_PARAMETERS, BT_SCAN_ENABLE, BT_SCAN_DISABLE
};
enum bt_result {
    BT_MORE = 0, BT_COMPLETE = 1, BT_UNRELATED = 2,
    BT_BAD_FRAME = -1, BT_CONTROLLER_ERROR = -2, BT_UNEXPECTED_STATUS = -3
};
struct bt_frame { uint8_t bytes[259]; size_t length; uint16_t opcode; };
struct bt_event_parser {
    uint8_t stage, event, length, used;
    uint8_t payload[255];
    int failed;
};
struct bt_reply {
    uint16_t opcode;
    uint8_t status, credits;
    uint8_t data[251];
    size_t length;
};
/* 纯内存封装：不包含发送、串口访问、HCI Reset 或 vendor 命令。 */
int bt_build(enum bt_command command, struct bt_frame *frame);
void bt_parser_init(struct bt_event_parser *parser);
int bt_feed(struct bt_event_parser *parser, uint8_t byte,
            uint16_t expected_opcode, struct bt_reply *reply);
int bt_validate_reply(enum bt_command command, const struct bt_reply *reply);
/* 只统计报告数及名称前缀 X2D；不输出/保存地址、完整名称或原始报告。 */
int bt_count_x2d(const uint8_t *event_payload, size_t length,
                 unsigned *reports, unsigned *x2d_names);
int bt_self_test(void);
#endif
