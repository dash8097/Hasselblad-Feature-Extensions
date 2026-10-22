/* 内存预拍核心。无固件地址、文件 I/O、设备调用或内部线程。
 * 调用者提供独占存储；init/destroy 时必须保证没有并发调用。
 * 输入平面必须是已完成 DMA 同步、在 push 返回前有效的 CPU 可读内存。
 * 所有时间采用同一单调时钟的纳秒，不可直接混用固件未知时间戳。
 */
#ifndef X2D_PRECAPTURE_RING_H
#define X2D_PRECAPTURE_RING_H
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#define PC_WIDTH 1600u
#define PC_HEIGHT 1200u
#define PC_CAPACITY 60u
#define PC_WINDOW_NS UINT64_C(1000000000)
#define PC_FRAME_BYTES ((size_t)PC_WIDTH * PC_HEIGHT * 3u / 2u)
#define PC_STORAGE_BYTES (PC_FRAME_BYTES * PC_CAPACITY)
enum pc_result { PC_OK, PC_BUSY, PC_INVALID, PC_DISABLED, PC_FROZEN, PC_EMPTY, PC_STALE, PC_CLOCK_RESET, PC_PENDING, PC_CAPTURE_BUSY };
struct pc_frame {
    const unsigned char *y, *uv;
    size_t y_bytes, uv_bytes, y_stride, uv_stride;
    uint32_t width, height;
    uint64_t timestamp_ns, sequence, stream_epoch;
};
struct pc_meta { uint64_t timestamp_ns, sequence, stream_epoch; };
struct pc_snapshot { uint64_t token, trigger_ns; uint32_t count; };
struct pc_ring {
    atomic_flag lock;
    atomic_uint_fast64_t pending_trigger;
    unsigned char *storage;
    struct pc_meta meta[PC_CAPACITY];
    uint32_t head, count;
    int enabled, frozen, have_sequence;
    uint64_t now_ns, last_sequence, last_timestamp, epoch, token, trigger_ns;
};
int pc_init(struct pc_ring *, void *storage, size_t bytes);
/* 只有显式启用才采集。关闭清除滚动队列，但不破坏待保存快照。
 * 冻结期间仅更新enabled；epoch仍属原快照。release后需重新enable指定新流epoch。
 * 进入参数页/息屏/流停止时，适配层必须停止输入；本核心不唤醒相机或开启取景。
 * 停用并等待所有调用退出后，调用方才可释放自有存储，不能释放任何原厂帧。 */
int pc_enable(struct pc_ring *, int enabled, uint64_t now_ns, uint64_t epoch);
int pc_push_nv12(struct pc_ring *, const struct pc_frame *, uint64_t now_ns);
/* 空闲无新帧时也应由普通后台定时调用，过期淘汰只移动索引，不释放原厂帧。 */
int pc_expire(struct pc_ring *, uint64_t now_ns);
/* 全按回调立即登记原始单调时间，不等待复制或文件 I/O。OK 只表示请求已登记。
 * 登记后阻止新的滚动/过期/模式重置；已经通过检查的操作允许完成。
 * 模式启停须由调用方协调，不能与全按并发清队列。关闭返回 PENDING 时尚未关闭。
 * 回调延迟送达导致队列已越过触发时间时，完成接口返回 STALE，不能伪称完整。
 * UINT64_MAX 保留为“无请求”。不得用轮询时刻代替原始全按时刻。 */
int pc_request_freeze(struct pc_ring *, uint64_t trigger_ns);
/* 后台完成已登记的请求；PENDING 保持原请求，可继续轮询。 */
int pc_complete_freeze(struct pc_ring *, struct pc_snapshot *);
/* 登记并尝试完成；返回 PENDING 后只调用 complete，不重新登记。 */
int pc_freeze(struct pc_ring *, uint64_t trigger_ns, struct pc_snapshot *);
/* 导出线程逐帧复制，冻结期间不做编码。index 按时间先后，目标不可与存储重叠。 */
int pc_copy_frozen(struct pc_ring *, uint64_t token, uint32_t index,
                   void *destination, size_t bytes, struct pc_meta *);
/* 只有保存成功或明确取消才能释放；失败可使用同一 token 重试读取。 */
int pc_release(struct pc_ring *, uint64_t token, uint64_t now_ns);
#endif
