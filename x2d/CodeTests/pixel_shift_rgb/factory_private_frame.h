/* 官方 4.2.0 的进程内私有 RAW 写入候选。不是已安装的拍摄入口。
 * 外层必须先建立独占 multishot 子目录并确认当前曝光属于本任务。
 * 不把对象加入原厂 Cache；仍由原厂生成元数据、保持共享内存和延迟回复。
 */
#ifndef FACTORY_PRIVATE_FRAME_H
#define FACTORY_PRIVATE_FRAME_H
#include <stdint.h>
#include <stddef.h>

typedef struct { void *value,*control; } X2dQtShared;
typedef struct { void *allocation; uint16_t *data; intptr_t size; } X2dQtString;
typedef struct {
 uint32_t exposure_id;
 uint8_t kind,padding[3];
 X2dQtShared image;
 void *metadata_map;
 uint8_t dump_preview;
} X2dExposureInfo;
_Static_assert(sizeof(X2dQtShared)==16,"Qt shared-pointer profile requires 64-bit");
_Static_assert(sizeof(X2dQtString)==24,"Qt string profile differs");
_Static_assert(offsetof(X2dExposureInfo,image)==8,"Exposure image offset differs");
_Static_assert(offsetof(X2dExposureInfo,metadata_map)==24,"Exposure metadata offset differs");
_Static_assert(offsetof(X2dExposureInfo,dump_preview)==32,"Exposure dump flag offset differs");

typedef struct X2dPrivateNativeApi X2dPrivateNativeApi;
typedef struct X2dPrivateFrame X2dPrivateFrame;
typedef struct {
 /* 所有回调在原厂存储 QObject 线程调用。不得在完成回调中销毁 frame。 */
 int (*destination_ready)(void *context,const char *path);
 int (*verify_finished_raw)(void *context,const char *path);
 void (*changed)(void *context,X2dPrivateFrame *frame);
 void *context;
} X2dPrivateFrameCallbacks;
enum { X2D_FRAME_EMPTY, X2D_FRAME_PENDING, X2D_FRAME_READY, X2D_FRAME_FAILED,
       X2D_FRAME_MEMORY_DRAINED };
enum {
 X2D_FRAME_FAILURE_NONE,
 X2D_FRAME_FAILURE_THREAD,
 X2D_FRAME_FAILURE_FLUSH_STATE,
 X2D_FRAME_FAILURE_DUPLICATE_FLUSH,
 X2D_FRAME_FAILURE_WORK_SHARED,
 X2D_FRAME_FAILURE_WORK_TYPE,
 X2D_FRAME_FAILURE_WORK_PATH,
 X2D_FRAME_FAILURE_DESTINATION,
 X2D_FRAME_FAILURE_STORED_RAW,
 X2D_FRAME_FAILURE_MEMORY_ROUTE
};
struct X2dPrivateNativeApi {
 uintptr_t base;
 void *(*qobject_thread)(const void *);
 void *(*current_thread)(void);
 int (*disconnect)(const void *);
 void (*connection_destroy)(void *);
};
struct X2dPrivateFrame {
 X2dPrivateNativeApi *api;
 X2dPrivateFrameCallbacks callbacks;
 X2dQtShared content,item;
 void *storage_io;
 void *connections[2];
 uint32_t exposure_id;
 unsigned status,submitted,preview_received,image_received,metadata_received;
 unsigned failure_reason;
 char path[256];
 /* 只读旁路；不能销毁 frame、改变通知或接管 WriteWork。 */
 void (*observe_raw_work)(void *,X2dPrivateFrame *,const X2dQtShared *);
 void *observe_context;
 /* 独立内存目的地：登记后不会回退 FileIO，也不报告原片写盘完成。 */
 int (*memory_raw_work)(void *,X2dPrivateFrame *,const X2dQtShared *);
 void *memory_context;
 unsigned memory_finished;
};
/* bind 只核对当前进程代码与 Qt ABI；不修改虚表、不控制设备。 */
int x2d_private_frame_bind(X2dPrivateNativeApi *api,uintptr_t storage_base);
typedef struct {
 uint32_t frames,sequence;
 uint8_t drive_mode,image_format;
} X2dCaptureIdentity;
/* 仅解码真实拍摄 metadata 中的 SnapshotMetadata；没有 I/O 或曝光操作。
 * api 必须已通过 bind。没有快照时返回 0，不能用 3FR 平面参数猜测身份。 */
int x2d_capture_metadata_identity(const X2dPrivateNativeApi *api,
 const void *metadata_map,X2dCaptureIdentity *identity);
int x2d_private_frame_create(X2dPrivateFrame *frame,X2dPrivateNativeApi *api,
 void *original_cache,uint32_t exposure_id,const char *path,X2dPrivateFrameCallbacks callbacks);
int x2d_private_frame_receive(X2dPrivateFrame *frame,const X2dExposureInfo *info,
 const X2dQtShared *notification);
/* 在任何图像/元数据输入前登记；回调在已校验的真实 RAW flush 信号中执行。
 * 原厂写入继续执行，观察失败由旁路自行取消，不伪造原厂完成。 */
int x2d_private_frame_observe_work(X2dPrivateFrame *,
 void (*)(void *,X2dPrivateFrame *,const X2dQtShared *),void *);
/* 必须在任何输入之前登记。外层须用本地通知代替原厂文件通知，独立持有
 * 真实通知，等内存消费者结束后回复真实结果。失败时也不写六张原片。 */
int x2d_private_frame_route_memory(X2dPrivateFrame *,
 int (*)(void *,X2dPrivateFrame *,const X2dQtShared *),void *);
/* 调用方已停止全部内存借用，并核对原厂头部工作已就绪，才允许收尾。
 * 这是消费者结束状态，不调用 fileFlushed，不改变原厂 isFlushed。 */
int x2d_private_frame_finish_memory(X2dPrivateFrame *,int success);
/* 只允许已结束写入，或尚未收到任何输入的对象被释放；挂起对象不能强行释放。 */
int x2d_private_frame_release(X2dPrivateFrame *frame);
#endif
