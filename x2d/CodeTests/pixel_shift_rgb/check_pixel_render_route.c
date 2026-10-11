/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pixel_render_route.h"
int main(void){
 PoRenderLease l={PO_RENDER_MAGIC,123,0,321,31000,11,22};
 assert(po_render_lease_valid(&l,sizeof l,1000,1,321));
 assert(!po_render_lease_valid(&l,sizeof l-1,1000,1,321));
 assert(!po_render_lease_valid(&l,sizeof l,999,1,321));
 assert(!po_render_lease_valid(&l,sizeof l,31000,1,321));
 assert(!po_render_lease_valid(&l,sizeof l,1000,0,321));
 assert(!po_render_lease_valid(&l,sizeof l,1000,1,322));
 assert(po_render_group(&l,12,11,22)==11);
 assert(po_render_group(&l,11,11,22)==11);
 assert(po_render_group(&l,12,22,11)==12);
 assert(po_render_group(&l,12,11,23)==12);
 l.magic=UINT64_C(0x3154523430443258);assert(!po_render_lease_valid(&l,sizeof l,1000,1,321));
 l.magic=PO_RENDER_MAGIC;
 l.input=0;assert(!po_render_lease_valid(&l,sizeof l,1000,1,321));l.input=11;
 l.output=11;assert(!po_render_lease_valid(&l,sizeof l,1000,1,321));l.output=22;
 uint64_t ticks=0;
 const char *s="123 (a ) b) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 321 900 1\n";
 assert(po_render_start_ticks(s,strlen(s),&ticks)&&ticks==321);
 assert(!po_render_start_ticks("123 wrong",9,&ticks));
 l.reserved=1;assert(!po_render_lease_valid(&l,sizeof l,1000,1,321));
 puts("RENDER_ROUTE_IDENTITY_EXPIRY_AND_EXACT_BUFFER_GUARDS_OK");return 0;
}
