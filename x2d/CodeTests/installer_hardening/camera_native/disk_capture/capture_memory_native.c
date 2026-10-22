#include "capture_memory_native.h"
#include "storage_private_bindings.inc"
typedef struct {uintptr_t function;void *result;uintptr_t args[8];} CmNativeCall;
typedef struct {void *allocation;const uint8_t *data;intptr_t length;} CmNativeBytes;
extern void x2d_qt_native_call(const CmNativeCall *);
#ifndef X2D_CAPTURE_MEMORY_HOST_CHECK
#define CM_FN(n,offset,type) ((type)((n)->api.base+(offset)))
#else
extern void *cm_mock_symbol(uintptr_t);
#define CM_FN(n,offset,type) ((type)cm_mock_symbol((n)->api.base+(offset)))
#endif
static int owner_thread(CmNative *n){
 return n&&n->api.base&&n->storage_owner&&
  n->api.qobject_thread(n->storage_owner)==n->api.current_thread();
}
static int allowed(void *context){return owner_thread(context);}
static int retain(void *context,const X2dQtShared *image,X2dQtShared *out){
 CmNative *n=context;
 if(!owner_thread(n)||!image||!out||out->value||out->control||!image->value||!image->control)return 0;
 __atomic_fetch_add((uint32_t*)image->control,1,__ATOMIC_RELAXED);
 __atomic_fetch_add((uint32_t*)image->control+1,1,__ATOMIC_RELAXED);
 *out=*image;return 1;
}
/* 消费线程不能直接调用 release；外层须将完成租约交回存储 QObject 线程。
 * 核心 cm_* 同样只能在这条序列执行，避免跨线程销毁原厂共享对象。 */
static void release(void *context,X2dQtShared *image){
 CmNative *n=context;
 if(!owner_thread(n)||!image||!image->control)return;
 CM_FN(n,B_SHARED_DESTROY,void(*)(X2dQtShared*))(image);memset(image,0,sizeof *image);
}
static int view(void *context,const X2dQtShared *image,CmView *out){
 CmNative *n=context;
 if(!owner_thread(n)||!image||!out||!image->value||!image->control)return 0;
 const uint8_t *object=image->value;CmNativeBytes bytes={0};
 CmNativeCall call={.function=n->api.base+B_IMAGE_RAW_DATA,.result=&bytes,.args={(uintptr_t)image->value}};
 x2d_qt_native_call(&call);
 if(bytes.allocation||!bytes.data||bytes.length<0)return 0;
 uint32_t content=CM_FN(n,B_IMAGE_DATA_SIZE,uint32_t(*)(const void*))(image->value);
 /* rawData 可包含分配容量；仅检查原厂 dataSize 标出的有效编码/像素区域。 */
 if(!content||(uint64_t)content>(uint64_t)bytes.length)return 0;
 memcpy(&out->width,object+0x70,4);memcpy(&out->height,object+0x74,4);
 out->format=object[0x7c];out->stride=CM_FN(n,B_IMAGE_STRIDE,uint32_t(*)(const void*))(image->value);
 out->data=bytes.data;out->bytes=content;out->owns_allocation=0;return 1;
}
int cm_native_bind(CmNative *n,uintptr_t base,void *owner,CmApi *out){
 if(!n||!out||!owner||!x2d_private_frame_bind(&n->api,base))return 0;
 n->storage_owner=owner;if(!owner_thread(n)){memset(n,0,sizeof *n);return 0;}
 *out=(CmApi){retain,release,view,n,allowed};return 1;
}
