#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 固定4.2.0自有后端只读通道；没有HTTP、任意路径或写文件入口。 */
enum X2dFaceEndpoint { X2D_FACE_BACKEND=0, X2D_FACE_EYE=1 };
typedef struct {uint64_t id,deadline_ms;int endpoint;} X2dFaceRequest;
typedef struct {uint64_t id;int endpoint,status,error;size_t length;char body[4096];} X2dFaceReply;
typedef struct X2dFaceIo X2dFaceIo;
uint64_t x2d_face_io_now_ms(void);
X2dFaceIo* x2d_face_io_create(void);
/* GUI单生产/消费线程。队列满返回0，调用方保留请求或放弃读取，不能阻塞GUI。 */
int x2d_face_io_submit(X2dFaceIo*,const X2dFaceRequest*);
int x2d_face_io_poll(X2dFaceIo*,X2dFaceReply*);
int x2d_face_io_cancel(X2dFaceIo*,uint64_t id);
void x2d_face_io_stop(X2dFaceIo*);
/* 停止后轮询release；1表示指针失效，0表示工作线程尚未退出。 */
int x2d_face_io_release(X2dFaceIo*);
/* 同步入口仅工作线程/离线测试可用。 */
void x2d_face_io_execute(const X2dFaceRequest*,X2dFaceReply*);
#ifdef __cplusplus
}
#endif
