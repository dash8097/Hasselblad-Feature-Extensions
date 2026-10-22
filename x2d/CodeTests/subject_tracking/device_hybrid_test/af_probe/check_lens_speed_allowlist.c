#include <assert.h>
#include <stdio.h>
#include "lens_speed_allowlist.h"
int main(void) {
    const uint32_t keys[]={0x12323,0x13131,0x13e3e,0x14f4f,0x24f4f,0x32727,0x14949,0x12f1c,0x1532f};
    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);i++) {
        assert(x2d_speed_lens_allowed(keys[i]));
        assert(!x2d_speed_lens_allowed(keys[i]|0x1000000u));
    }
    unsigned count=0;
    for(uint32_t k=0;k<0x1000000;k++)count+=x2d_speed_lens_allowed(k)!=0;
    assert(count==9); /* 八种镜头，90V 两种身份。 */
    assert(!x2d_speed_lens_allowed(0));assert(!x2d_speed_lens_allowed(UINT32_MAX));
    assert(x2d_speed_lens_allowed(0x13e3e));assert(!x2d_speed_lens_allowed(0x12d2d));
    puts("PASS: eight lens models, nine identities; all 24-bit keys classified; unknown/changed lens denied");
    return 0;
}
