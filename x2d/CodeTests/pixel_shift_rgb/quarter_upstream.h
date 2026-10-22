/* 固定4.2.0前编码适配；原厂图像引用由原调用栈持有。 */
#ifndef QUARTER_UPSTREAM_H
#define QUARTER_UPSTREAM_H
#include "quarter_direct_jpeg.h"
typedef struct {void *frame,*stream;uint32_t quality,reserved;} QduRequest;
typedef struct {
 void *context;
 int (*eligible)(void *,const void *frame); /* 已对应曝光且存储端快照明确是25MP */
 int (*size)(void *,uint32_t *);
 int (*map)(void *,void **);
 int (*unmap)(void *);
 int (*sync)(void *,int);
 int (*allocate)(void *,void **,uint32_t);
 int (*release)(void *,void *);
 int (*encode)(void *,QduRequest *);
} QduOps;
typedef struct {void *quarantined;unsigned blocked,direct,fallback,failed;} QduState;
/* 调用方串行化本state。HAL错误后保留私有DMA输入、停用后续直编，不能
 * 假定任意错误返回都代表DMA已停止；不得紧接着重试原尺寸编码。 */
int qdu_encode(QduState *,const QduOps *,void *encoder,QduRequest *);
/* 调用方持有原厂engine+0x198互斥锁；只读真实QMap，不修改队列或引用。 */
int qdu_exposure(const void *engine,uint32_t frame,uintptr_t expected_functor,uint32_t *exposure);
#endif
