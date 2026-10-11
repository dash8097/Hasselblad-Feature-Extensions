#include "bt_hci.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
static int feed(const uint8_t *data, size_t n, uint16_t op, struct bt_reply *r) {
    struct bt_event_parser p;
    int v = BT_MORE;
    bt_parser_init(&p);
    for (size_t i=0; i<n; ++i) { v=bt_feed(&p,data[i],op,r); if (v<0) break; }
    return v;
}
int bt_self_test(void) {
    struct bt_frame f;
    struct bt_reply r;
    struct bt_event_parser parser;
    const uint8_t version[] = {4,14,12,1,1,16,0,6,0,0,6,15,0,0,0};
    const uint8_t empty_reply[] = {4,14,4,1,10,32,0};
    const uint8_t command_status[] = {4,15,4,0,1,10,32};
    const uint8_t reject[] = {4,14,4,1,10,32,12};
    const uint8_t short_complete[] = {4,14,3,1,10,32};
    uint8_t invalid[] = {0,14,4,1,10,32,0};
    unsigned reports=99,names=99;
    /* 合成地址全为0，不保存真实附近设备身份。 */
    const uint8_t report[] = {2,1,0,0,0,0,0,0,0,0,5,4,9,'X','2','D',0xc0};
    uint8_t bad_report[sizeof(report)];
    for (int i=BT_VERSION; i<=BT_SCAN_DISABLE; ++i) {
        CHECK(bt_build((enum bt_command)i,&f)==0);
        CHECK(f.length==(size_t)f.bytes[3]+4 && f.bytes[0]==1);
        CHECK(f.opcode!=0x0c03 && (f.opcode >> 10)!=0x3f);
    }
    CHECK(bt_build((enum bt_command)100,&f)==-1);
    CHECK(bt_build(BT_VERSION,&f)==0);
    CHECK(f.length==4 && !memcmp(f.bytes,"\x01\x01\x10\x00",4));
    CHECK(feed(version,sizeof(version),f.opcode,&r)==BT_COMPLETE);
    CHECK(bt_validate_reply(BT_VERSION,&r)==0);
    CHECK(bt_validate_reply(BT_FEATURES,&r)==-1);
    for (size_t i=0;i<sizeof(version);++i) CHECK(feed(version,i,0x1001,&r)==BT_MORE);
    CHECK(feed(version,sizeof(version),0x200a,&r)==BT_UNRELATED);
    CHECK(feed(empty_reply,sizeof(empty_reply),0x200a,&r)==BT_COMPLETE);
    CHECK(bt_validate_reply(BT_ADV_ENABLE,&r)==0);
    CHECK(feed(command_status,sizeof(command_status),0x200a,&r)==BT_UNEXPECTED_STATUS);
    CHECK(feed(reject,sizeof(reject),0x200a,&r)==BT_CONTROLLER_ERROR);
    CHECK(feed(short_complete,sizeof(short_complete),0x200a,&r)==BT_BAD_FRAME);
    CHECK(feed(invalid,sizeof(invalid),0x200a,&r)==BT_BAD_FRAME);
    bt_parser_init(&parser);
    CHECK(bt_feed(&parser,2,0x1001,&r)==BT_BAD_FRAME);
    CHECK(bt_feed(&parser,4,0x1001,&r)==BT_BAD_FRAME);
    CHECK(bt_build(BT_ADV_PARAMETERS,&f)==0 && f.bytes[8]==3 && f.bytes[17]==7);
    CHECK(bt_build(BT_ADV_DATA,&f)==0 && f.length==36);
    CHECK(f.bytes[4]==16 && !memcmp(f.bytes+10,"X1D-BT-Test",11));
    CHECK(bt_build(BT_PASSIVE_SCAN_PARAMETERS,&f)==0 && f.bytes[4]==0);
    CHECK(bt_build(BT_ADV_DISABLE,&f)==0 && f.bytes[4]==0);
    CHECK(bt_build(BT_SCAN_DISABLE,&f)==0 && f.bytes[4]==0);
    CHECK(bt_count_x2d(report,sizeof(report),&reports,&names)==0 && reports==1 && names==1);
    for (size_t i=0;i<sizeof(report);++i) CHECK(bt_count_x2d(report,i,&reports,&names)==-1);
    memcpy(bad_report,report,sizeof(report)); bad_report[11]=31;
    CHECK(bt_count_x2d(bad_report,sizeof(bad_report),&reports,&names)==-1);
    memcpy(bad_report,report,sizeof(report)); bad_report[13]='Y';
    CHECK(bt_count_x2d(bad_report,sizeof(bad_report),&reports,&names)==0 && names==0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc==2 && !strcmp(argv[1],"--self-test")) {
        int result=bt_self_test();
        if (result) { printf("self_test_failed_line=%d\n",result); return 1; }
        puts("offline_hci_self_test=passed;hardware_access=none"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--status")) {
        puts("binding=unverified;radio=not_started;stage=offline_protocol_candidate"); return 0;
    }
    /* r1 没有设备访问代码；任何 live 参数均在打开设备前拒绝。 */
    fputs("hardware_binding_unverified: live operation unavailable\n",stderr);
    return 2;
}
