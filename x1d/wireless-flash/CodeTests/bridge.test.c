#include "../native/rf_bridge.h"
#include <assert.h>
#include <stdio.h>
static unsigned checks=0;
#define CHECK(test) do { assert(test); ++checks; } while(0)
int main(void) {
    RfBridgePacket p,q;
    CHECK(sizeof(p)==256);
    rf_bridge_init(&p,RF_UI_CONFIG,9,2,UINT64_C(0x123456789));
    p.values[RV_POWER]=60;
    CHECK(rf_bridge_valid(&p,sizeof(p)));
    CHECK(rf_bridge_settings_valid(&p));
    CHECK(rf_bridge_time(&p)==UINT64_C(0x123456789));
    for (unsigned n=0;n<256;++n) CHECK(!rf_bridge_valid(&p,n));
    CHECK(!rf_bridge_valid(&p,257));
    q=p; q.session=0; CHECK(!rf_bridge_valid(&q,256));
    q=p; q.magic^=1; CHECK(!rf_bridge_valid(&q,256));
    q=p; q.version=1; CHECK(!rf_bridge_valid(&q,256));
    q=p; q.kind=17; CHECK(!rf_bridge_valid(&q,256));
    q=p; q.values[RV_ON]=2; CHECK(!rf_bridge_settings_valid(&q));
    q=p; q.values[RV_DELAY]=5001; CHECK(!rf_bridge_settings_valid(&q));
    q=p; q.values[RV_DELAY]=5000; CHECK(rf_bridge_settings_valid(&q));
    q=p; q.values[RV_POWER]=9; CHECK(!rf_bridge_settings_valid(&q));
    q=p; q.values[RV_POWER]=10; CHECK(rf_bridge_settings_valid(&q));
    q=p; q.values[RV_POWER]=100; CHECK(rf_bridge_settings_valid(&q));
    q=p; q.values[RV_POWER]=101; CHECK(!rf_bridge_settings_valid(&q));
    q=p; q.values[RV_POWER]=UINT32_MAX; CHECK(!rf_bridge_settings_valid(&q));
    q=p; q.values[RV_PAGE]=2; CHECK(!rf_bridge_settings_valid(&q));
    q=p; q.values[RV_SOURCE]=1; CHECK(rf_bridge_settings_valid(&q));
    q.values[RV_SOURCE]=2; CHECK(rf_bridge_settings_valid(&q));
    q.values[RV_SOURCE]=3; CHECK(!rf_bridge_settings_valid(&q));
    q=p; q.values[RV_PROGRESS]=100; CHECK(rf_bridge_settings_valid(&q));
    q.values[RV_PROGRESS]=101; CHECK(!rf_bridge_settings_valid(&q));
    printf("{\"passed\":%u,\"hardwareRequests\":0}\n",checks); return 0;
}
