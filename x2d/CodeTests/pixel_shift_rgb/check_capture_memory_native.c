/* 原厂适配调用替身：真实引用计数操作与存储线程约束，不连接相机。 */
#define X2D_CAPTURE_MEMORY_HOST_CHECK 1
#include <stdio.h>
#include <stdlib.h>
#include "capture_memory_native.c"
#include "private_capture_group.c"
#define CHECK(v) do{if(!(v)){fprintf(stderr,"CAPTURE_NATIVE_CHECK_FAILED line=%u\n",__LINE__);exit(1);}}while(0)
static uintptr_t test_base=0x100000;
static unsigned on_owner=1,profile_ok=1,destroyed,borrowed;
static uint8_t image[0x80],*data;
static uint32_t counts[2];
static void *object_thread(const void *owner){CHECK(owner==image);return (void*)(uintptr_t)1;}
static void *current_thread(void){return (void*)(uintptr_t)(on_owner?1:2);}
int x2d_private_frame_bind(X2dPrivateNativeApi *api,uintptr_t base){
 if(!profile_ok||base!=test_base)return 0;
 *api=(X2dPrivateNativeApi){.base=base,.qobject_thread=object_thread,.current_thread=current_thread};return 1;
}
static uint32_t stride(const void *p){CHECK(p==image);return 23808;}
static uint32_t size(const void *p){CHECK(p==image);return 210510336;}
static void shared_destroy(X2dQtShared *p){
 CHECK(on_owner&&p->value==image&&p->control==counts&&counts[0]>1&&counts[1]>1);
 counts[0]--;counts[1]--;destroyed++;
}
void *cm_mock_symbol(uintptr_t address){
 if(address==test_base+B_IMAGE_STRIDE)return stride;
 if(address==test_base+B_IMAGE_DATA_SIZE)return size;
 if(address==test_base+B_SHARED_DESTROY)return shared_destroy;
 CHECK(0);return NULL;
}
void x2d_qt_native_call(const CmNativeCall *call){
 CHECK(on_owner&&call->function==test_base+B_IMAGE_RAW_DATA&&call->args[0]==(uintptr_t)image);
 *(CmNativeBytes*)call->result=(CmNativeBytes){NULL,data,210510336};borrowed++;
}
int main(void){
 data=malloc(210510336);CHECK(data);uint32_t width=11836,height=8842;
 memcpy(image+0x70,&width,4);memcpy(image+0x74,&height,4);image[0x7c]=1;
 counts[0]=counts[1]=1;CmNative n={0};CmApi api={0};
 profile_ok=0;CHECK(!cm_native_bind(&n,test_base,image,&api));profile_ok=1;
 on_owner=0;CHECK(!cm_native_bind(&n,test_base,image,&api));on_owner=1;
 CHECK(cm_native_bind(&n,test_base,image,&api));CmInput p;CHECK(cm_init(&p,5,api));
 CHECK(x2d_capture_group_request_issued(&p.group,5));
 X2dCaptureIdentity identity={6,0,6,0};X2dExposureInfo info={.exposure_id=91,.kind=1,.image={image,counts}};
 CHECK(cm_receive(&p,&info,&identity,1)==X2D_CAPTURE_OWN);
 CHECK(counts[0]==2&&counts[1]==2&&borrowed==1&&!destroyed);
 CmSlice slice;CHECK(cm_rows_take(&p,0,1024,&slice));
 on_owner=0;CHECK(!cm_rows_done(&p,0,5,slice.ticket,1));cm_abort(&p);
 CHECK(p.frames[0].active&&!p.failed&&counts[0]==2&&!destroyed);
 on_owner=1;cm_abort(&p);CHECK(p.failed&&counts[0]==2);
 CHECK(cm_rows_done(&p,0,5,slice.ticket,1));CHECK(cm_drained(&p));
 CHECK(counts[0]==1&&counts[1]==1&&destroyed==1);free(data);
 puts("NATIVE_BORROW_STRONG_REFERENCE_AND_OWNER_THREAD_COMPLETION_PASSED_OFFLINE_ONLY");return 0;
}
