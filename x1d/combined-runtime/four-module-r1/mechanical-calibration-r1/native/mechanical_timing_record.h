#ifndef HBL_MECHANICAL_TIMING_RECORD_H
#define HBL_MECHANICAL_TIMING_RECORD_H
#include <stdint.h>
#include <string.h>

/* 只记录调用方已取得的值；没有时钟换算、补偿、文件或设备访问。
 * 单线程 worker 所有，禁止跨线程使用。满环覆盖最旧记录并显式累计。
 */
#define HBL_TIMING_CAPACITY 2048u
#define HBL_TIMING_MAGIC UINT32_C(0x31544248) /* HBT1 */
enum HblTimingKind {
    HBL_TIMING_CONFIG=1, HBL_TIMING_SAMPLE=2, HBL_TIMING_ACCEPT=3,
    HBL_TIMING_DUE=4, HBL_TIMING_SKIP=5, HBL_TIMING_SUBMIT_BEGIN=6,
    HBL_TIMING_SUBMIT_END=7, HBL_TIMING_REPLY=8, HBL_TIMING_FAILURE=9,
    HBL_TIMING_CANCEL=10
};
typedef struct {
    uint64_t mono_us, observer_ns, hardware_ticks, deadline_us;
    uint32_t kind, trial, epoch, source, delay_us, sequence, detail;
    uint32_t timer_control, sync_status, clear_flags;
} HblTimingEvent;
typedef struct {
    HblTimingEvent events[HBL_TIMING_CAPACITY];
    uint32_t next, count, overwritten, dirty;
    uint64_t total;
} HblTimingRing;

static inline void hbl_timing_push(HblTimingRing *r, const HblTimingEvent *event)
{
    if (!r || !event) return;
    r->events[r->next] = *event;
    r->next = (r->next + 1) % HBL_TIMING_CAPACITY;
    if (r->count < HBL_TIMING_CAPACITY) ++r->count;
    else if (r->overwritten != UINT32_MAX) ++r->overwritten;
    if (r->total != UINT64_MAX) ++r->total;
    r->dirty = 1;
}
static inline uint32_t hbl_timing_first(const HblTimingRing *r)
{
    return r->count == HBL_TIMING_CAPACITY ? r->next : 0;
}
/* 仅在自动引闪关闭、无待发截止点、无线操作空闲时允许落盘。 */
static inline int hbl_timing_can_flush(const HblTimingRing *r,
                                      unsigned enabled, unsigned pending, unsigned busy)
{
    return r && r->dirty && !enabled && !pending && !busy;
}
#endif
