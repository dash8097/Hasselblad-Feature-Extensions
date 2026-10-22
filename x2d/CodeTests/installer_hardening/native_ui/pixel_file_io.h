#ifndef X2D_PIXEL_FILE_IO_H
#define X2D_PIXEL_FILE_IO_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 与 PixelEndpoint 顺序保持一致；无任意路径、URL 或捕获 request 入口。 */
enum X2dPixelEndpoint { X2D_PIXEL_STATUS, X2D_PIXEL_SHUTTER, X2D_PIXEL_QUARTER_STATUS,
 X2D_PIXEL_ARM, X2D_PIXEL_QUARTER, X2D_PIXEL_FINISH, X2D_LIVEVIEW_PREFERENCES };
typedef struct { uint64_t id,deadline_ms; int endpoint,write; size_t length; char body[512]; } X2dPixelIoRequest;
typedef struct { uint64_t id; int status,error; size_t length; char body[16384]; } X2dPixelIoReply;
typedef struct X2dPixelIo X2dPixelIo;
/* create 只创建一个线程；submit/poll 不执行文件操作、不等待锁。单 GUI 调用方。 */
X2dPixelIo* x2d_pixel_io_create(void);
int x2d_pixel_io_submit(X2dPixelIo*,const X2dPixelIoRequest*);
int x2d_pixel_io_poll(X2dPixelIo*,X2dPixelIoReply*);
/* deadline与此单调时钟同源；submit的0期限默认5秒，不应延长已有请求期限。 */
uint64_t x2d_pixel_io_now_ms(void);
int x2d_pixel_io_cancel(X2dPixelIo*,uint64_t id);
/* 丢弃尚未开始的队列；已开始的写入可能完成，不以关闭推定取消成功。
 * release 返回0须后续再试，返回1后指针失效；不得并发使用。 */
void x2d_pixel_io_stop(X2dPixelIo*);
int x2d_pixel_io_release(X2dPixelIo*);
/* 同步底层仅供专属工作线程/隔离测试调用；不在 GUI 线程调用。 */
void x2d_pixel_io_execute(const X2dPixelIoRequest*,X2dPixelIoReply*);
#ifdef __cplusplus
}
#endif
#endif
