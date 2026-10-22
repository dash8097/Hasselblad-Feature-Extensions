/* 有界、非阻塞记录。竞争时丢记录并计数，不等待相机工作线程。 */
#ifndef X2D_AF_PROBE_CORE_H
#define X2D_AF_PROBE_CORE_H
#include <stdint.h>
#include <stdatomic.h>
#define AF_EVENT_CAPACITY 256
enum { AF_BEGIN=1, AF_BRANCH, AF_SPEED, AF_SCAN_QUEUE, AF_POSITION_QUEUE,
       AF_STOP_QUEUE, AF_SCAN_DRIVER, AF_POSITION_DRIVER, AF_CDAF_END, AF_LENS_SEND, AF_NEAR_GUARD };
typedef struct { uint64_t us, caller; int kind, context, a, b, result, flags, phase; unsigned request, send_order, speed_type_plus1; } AfEvent;
typedef struct {
    atomic_flag busy;
    atomic_uint dropped;
    unsigned count, session;
    AfEvent events[AF_EVENT_CAPACITY];
} AfBuffer;
typedef struct { unsigned count, session, dropped; AfEvent events[AF_EVENT_CAPACITY]; } AfSnapshot;
static AfBuffer af_buffer={.busy=ATOMIC_FLAG_INIT};
static void af_record(AfEvent e) {
    if(atomic_flag_test_and_set_explicit(&af_buffer.busy,memory_order_acquire)) {
        atomic_fetch_add_explicit(&af_buffer.dropped,1,memory_order_relaxed); return;
    }
    if(e.kind==AF_BEGIN) {
        af_buffer.count=0; ++af_buffer.session;
        atomic_store_explicit(&af_buffer.dropped,0,memory_order_relaxed);
    }
    if(af_buffer.count<AF_EVENT_CAPACITY)af_buffer.events[af_buffer.count++]=e;
    else atomic_fetch_add_explicit(&af_buffer.dropped,1,memory_order_relaxed);
    atomic_flag_clear_explicit(&af_buffer.busy,memory_order_release);
}
static int af_snapshot(AfSnapshot *s) {
    if(atomic_flag_test_and_set_explicit(&af_buffer.busy,memory_order_acquire))return 0;
    s->count=af_buffer.count; s->session=af_buffer.session;
    s->dropped=atomic_load_explicit(&af_buffer.dropped,memory_order_relaxed);
    for(unsigned i=0;i<s->count;i++)s->events[i]=af_buffer.events[i];
    atomic_flag_clear_explicit(&af_buffer.busy,memory_order_release); return 1;
}
#endif
