/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "four_hundred_policy.h"
int main(void) {
    for(unsigned raw=0;raw<2;raw++)for(unsigned jpeg=0;jpeg<2;jpeg++) {
        struct four_hundred_output out={0,0};
        assert(four_hundred_output_policy(raw,jpeg,&out));
        assert(out.write_jpeg==1 && out.write_raw==raw);
    }
    struct four_hundred_output out={9,9};
    assert(!four_hundred_output_policy(2,0,&out));
    assert(out.write_jpeg==9 && out.write_raw==9);
    assert(!four_hundred_output_policy(0,2,&out));
    assert(!four_hundred_output_policy(0,0,0));
    struct four_hundred_pair good={1,1,1,1,23326,17498};
    assert(four_hundred_playback_policy(&good)==FOUR_HUNDRED_JPEG);
    assert(four_hundred_playback_policy(0)==FOUR_HUNDRED_ORIGINAL);
    for(unsigned flag=0;flag<4;flag++) {
        struct four_hundred_pair pair=good;
        if(flag==0)pair.owned_capture=0;
        if(flag==1)pair.committed=0;
        if(flag==2)pair.jpeg_validated=0;
        if(flag==3)pair.same_capture=0;
        assert(four_hundred_playback_policy(&pair)==(flag?FOUR_HUNDRED_UNAVAILABLE:FOUR_HUNDRED_ORIGINAL));
    }
    struct four_hundred_pair pair=good;pair.width=11656;pair.height=8742;
    assert(four_hundred_playback_policy(&pair)==FOUR_HUNDRED_UNAVAILABLE);
    pair=good;pair.owned_capture=0;
    assert(four_hundred_playback_policy(&pair)==FOUR_HUNDRED_ORIGINAL);
    pair=good;pair.jpeg_validated=2;
    assert(four_hundred_playback_policy(&pair)==FOUR_HUNDRED_UNAVAILABLE);
    puts("FOUR_HUNDRED_OUTPUT_AND_PLAYBACK_POLICY_PASSED");return 0;
}
