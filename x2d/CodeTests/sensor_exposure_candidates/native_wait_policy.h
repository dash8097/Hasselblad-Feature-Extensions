#ifndef X2D_NATIVE_WAIT_POLICY_H
#define X2D_NATIVE_WAIT_POLICY_H
#include <stdint.h>
/* perf descriptors wake ppoll on new records. The idle budget bounds command
 * polling; an already scheduled flash always retains its exact remaining ns. */
static inline uint64_t native_flash_wait_ns(unsigned sampling,unsigned pending,uint64_t deadline,uint64_t at) {
    uint64_t budget=sampling?10000000ull:50000000ull;
    if(pending){uint64_t remaining=deadline>at?deadline-at:0;if(remaining<budget)budget=remaining;}
    return budget;
}
#endif
