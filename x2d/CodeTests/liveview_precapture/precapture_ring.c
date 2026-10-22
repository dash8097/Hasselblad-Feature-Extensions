#include "precapture_ring.h"
#include <string.h>

static int take(struct pc_ring *r) { return !atomic_flag_test_and_set_explicit(&r->lock, memory_order_acquire); }
static int done(struct pc_ring *r, int result) { atomic_flag_clear_explicit(&r->lock,memory_order_release);return result; }
static int pending(struct pc_ring *r) { return atomic_load_explicit(&r->pending_trigger,memory_order_acquire)!=UINT64_MAX; }
static void clear(struct pc_ring *r) { r->head=r->count=0;r->have_sequence=0; }
static void trim(struct pc_ring *r, uint64_t now) {
    while (r->count && now >= r->meta[r->head].timestamp_ns && now-r->meta[r->head].timestamp_ns > PC_WINDOW_NS) {
        r->head=(r->head+1)%PC_CAPACITY;--r->count;
    }
}
static int clock_step(struct pc_ring *r,uint64_t now) {
    if(now<r->now_ns) { clear(r);r->now_ns=now;return 0; }
    r->now_ns=now;trim(r,now);return 1;
}
static int overlaps(const void *a,size_t n,const void *b,size_t m) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(n>UINTPTR_MAX-x || m>UINTPTR_MAX-y)return 1;
    return x<y+m && y<x+n;
}
static int plane(const void *p,size_t bytes,size_t stride,size_t rows) {
    return p && stride>=PC_WIDTH && stride<=SIZE_MAX/rows && bytes>=stride*(rows-1)+PC_WIDTH;
}
int pc_init(struct pc_ring *r,void *storage,size_t bytes) {
    if(!r || !storage || bytes<PC_STORAGE_BYTES || overlaps(r,sizeof(*r),storage,PC_STORAGE_BYTES))return PC_INVALID;
    memset(r,0,sizeof(*r));atomic_flag_clear(&r->lock);
    atomic_init(&r->pending_trigger,UINT64_MAX);
    if(!atomic_is_lock_free(&r->pending_trigger))return PC_INVALID;
    r->storage=storage;return PC_OK;
}
int pc_enable(struct pc_ring *r,int enabled,uint64_t now,uint64_t epoch) {
    if(!r || !r->storage)return PC_INVALID;
    if(!take(r))return PC_BUSY;
    if(!r->frozen && pending(r))return done(r,PC_PENDING);
    r->enabled=!!enabled;
    if(r->frozen)return done(r,PC_FROZEN);
    clear(r);r->now_ns=now;r->epoch=epoch;return done(r,PC_OK);
}
int pc_push_nv12(struct pc_ring *r,const struct pc_frame *f,uint64_t now) {
    if(!r || !r->storage || !f || f->width!=PC_WIDTH || f->height!=PC_HEIGHT ||
       !plane(f->y,f->y_bytes,f->y_stride,PC_HEIGHT) || !plane(f->uv,f->uv_bytes,f->uv_stride,PC_HEIGHT/2) ||
       overlaps(f->y,f->y_bytes,r->storage,PC_STORAGE_BYTES) || overlaps(f->uv,f->uv_bytes,r->storage,PC_STORAGE_BYTES))return PC_INVALID;
    if(!take(r))return PC_BUSY;
    if(r->frozen)return done(r,PC_FROZEN);
    if(pending(r))return done(r,PC_PENDING);
    if(!r->enabled)return done(r,PC_DISABLED);
    if(!clock_step(r,now))return done(r,PC_CLOCK_RESET);
    if(f->stream_epoch!=r->epoch)return done(r,PC_STALE);
    if(f->timestamp_ns>now || now-f->timestamp_ns>PC_WINDOW_NS)return done(r,PC_STALE);
    if(r->have_sequence && (f->sequence<=r->last_sequence || f->timestamp_ns<=r->last_timestamp))return done(r,PC_STALE);
    if(r->count==PC_CAPACITY){r->head=(r->head+1)%PC_CAPACITY;--r->count;}
    uint32_t ix=(r->head+r->count)%PC_CAPACITY;
    unsigned char *out=r->storage+(size_t)ix*PC_FRAME_BYTES;
#ifdef PC_TEST_HOOK
    extern void pc_test_copy_barrier(void);
    pc_test_copy_barrier();
#endif
    for(size_t row=0;row<PC_HEIGHT;++row)memcpy(out+row*PC_WIDTH,f->y+row*f->y_stride,PC_WIDTH);
    out+=(size_t)PC_WIDTH*PC_HEIGHT;
    for(size_t row=0;row<PC_HEIGHT/2;++row)memcpy(out+row*PC_WIDTH,f->uv+row*f->uv_stride,PC_WIDTH);
    r->meta[ix]=(struct pc_meta){f->timestamp_ns,f->sequence,f->stream_epoch};
    ++r->count;r->have_sequence=1;r->last_sequence=f->sequence;r->last_timestamp=f->timestamp_ns;
    return done(r,PC_OK);
}
int pc_expire(struct pc_ring *r,uint64_t now) {
    if(!r || !r->storage)return PC_INVALID;
    if(!take(r))return PC_BUSY;
    if(r->frozen)return done(r,PC_FROZEN);
    if(pending(r))return done(r,PC_PENDING);
    int ok=clock_step(r,now);return done(r,ok?PC_OK:PC_CLOCK_RESET);
}
int pc_request_freeze(struct pc_ring *r,uint64_t trigger) {
    if(!r || !r->storage || trigger==UINT64_MAX)return PC_INVALID;
    uint_fast64_t expected=UINT64_MAX;
    return atomic_compare_exchange_strong_explicit(&r->pending_trigger,&expected,trigger,
        memory_order_acq_rel,memory_order_acquire)?PC_OK:PC_CAPTURE_BUSY;
}
static int reject_request(struct pc_ring *r,int result) {
    atomic_store_explicit(&r->pending_trigger,UINT64_MAX,memory_order_release);
    return done(r,result);
}
int pc_complete_freeze(struct pc_ring *r,struct pc_snapshot *s) {
    if(!r || !r->storage || !s || overlaps(s,sizeof(*s),r,sizeof(*r)) ||
       overlaps(s,sizeof(*s),r->storage,PC_STORAGE_BYTES))return PC_INVALID;
    if(!take(r))return PC_PENDING;
    if(r->frozen)return done(r,PC_FROZEN);
    uint64_t trigger=atomic_load_explicit(&r->pending_trigger,memory_order_acquire);
    if(trigger==UINT64_MAX)return done(r,PC_EMPTY);
    if(!r->enabled)return reject_request(r,PC_DISABLED);
    /* 若全按事件晚于帧/过期操作到达，旧帧可能已经丢弃，明确拒绝不完整快照。 */
    if(trigger<r->now_ns || (r->have_sequence && trigger<=r->last_timestamp))
        return reject_request(r,PC_STALE);
    trim(r,trigger);
    if(!r->count)return reject_request(r,PC_EMPTY);
    if(r->token==UINT64_MAX)return reject_request(r,PC_INVALID);
    ++r->token;r->frozen=1;r->trigger_ns=trigger;
    *s=(struct pc_snapshot){r->token,trigger,r->count};return done(r,PC_OK);
}
int pc_freeze(struct pc_ring *r,uint64_t trigger,struct pc_snapshot *s) {
    if(!r || !r->storage || !s || overlaps(s,sizeof(*s),r,sizeof(*r)) ||
       overlaps(s,sizeof(*s),r->storage,PC_STORAGE_BYTES))return PC_INVALID;
    int rc=pc_request_freeze(r,trigger);
    return rc==PC_OK?pc_complete_freeze(r,s):rc;
}
int pc_copy_frozen(struct pc_ring *r,uint64_t token,uint32_t index,void *dest,size_t bytes,struct pc_meta *meta) {
    if(!r || !r->storage || !dest || !meta || bytes<PC_FRAME_BYTES ||
       overlaps(dest,PC_FRAME_BYTES,r->storage,PC_STORAGE_BYTES) || overlaps(dest,PC_FRAME_BYTES,r,sizeof(*r)) ||
       overlaps(meta,sizeof(*meta),r,sizeof(*r)) || overlaps(meta,sizeof(*meta),r->storage,PC_STORAGE_BYTES) ||
       overlaps(meta,sizeof(*meta),dest,PC_FRAME_BYTES))return PC_INVALID;
    if(!take(r))return PC_BUSY;
    if(!r->frozen || token!=r->token || index>=r->count)return done(r,PC_STALE);
    uint32_t ix=(r->head+index)%PC_CAPACITY;
    memcpy(dest,r->storage+(size_t)ix*PC_FRAME_BYTES,PC_FRAME_BYTES);*meta=r->meta[ix];
    return done(r,PC_OK);
}
int pc_release(struct pc_ring *r,uint64_t token,uint64_t now) {
    if(!r || !r->storage)return PC_INVALID;
    if(!take(r))return PC_BUSY;
    if(!r->frozen || token!=r->token)return done(r,PC_STALE);
    r->frozen=0;clear(r);r->now_ns=now;
    atomic_store_explicit(&r->pending_trigger,UINT64_MAX,memory_order_release);
    return done(r,PC_OK);
}
