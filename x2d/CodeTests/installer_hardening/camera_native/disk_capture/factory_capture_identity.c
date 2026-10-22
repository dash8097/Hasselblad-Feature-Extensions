/* 固定原厂 4.2.0 / Qt 6.4.1 的拍摄快照解码；只读对象数据，不发设备请求。
 * SnapshotMetadataSource::update 0x139ea4 分配 0xc8 字节后调用原厂构造函数。
 * 不把最终照片解析参数当作拍摄快照，也不推算 ExposureId。 */
#include "factory_private_frame.h"
#include "storage_private_bindings.inc"
#include <string.h>
typedef struct {uintptr_t function;void *result;uintptr_t args[8];} NativeCall;
extern void x2d_qt_native_call(const NativeCall *);
#ifndef X2D_CAPTURE_IDENTITY_HOST_CHECK
#define FN(api,offset,type) ((type)((api)->base+(offset)))
#else
extern void *x2d_identity_mock_symbol(uintptr_t);
#define FN(api,offset,type) ((type)x2d_identity_mock_symbol((api)->base+(offset)))
#endif
int x2d_capture_metadata_identity(const X2dPrivateNativeApi *api,
 const void *metadata_map,X2dCaptureIdentity *identity){
 if(identity)memset(identity,0,sizeof *identity);
 if(!api||!api->base||!metadata_map||!identity)return 0;
 _Alignas(16) uint8_t value[32]={0},invalid[32]={0};
 /* 原厂默认 QVariant 的 isNull 位在 +24 的第二位。 */
 invalid[24]=2;
 const void *tag=FN(api,B_SNAPSHOT_TAG,const void*(*)(void))();
 if(!tag)return 0;
 NativeCall find={.function=api->base+B_MAP_VALUE,.result=value,
  .args={(uintptr_t)metadata_map,(uintptr_t)tag,(uintptr_t)invalid}};
 x2d_qt_native_call(&find);
 void *snapshot_map=0;
 NativeCall convert={.function=api->base+B_VARIANT_MAP,.result=&snapshot_map,
  .args={(uintptr_t)value}};
 x2d_qt_native_call(&convert);
 FN(api,B_VARIANT_DESTROY,void(*)(void*))(value);
 if(!snapshot_map)return 0;
 void *snapshot=FN(api,B_OPERATOR_NEW,void*(*)(size_t))(0xc8);
 if(!snapshot){FN(api,B_MAP_DESTROY,void(*)(void*))(&snapshot_map);return 0;}
 FN(api,B_SNAPSHOT_CONSTRUCTOR,void(*)(void*,const void*,void*))(
  snapshot,&snapshot_map,0);
 FN(api,B_MAP_DESTROY,void(*)(void*))(&snapshot_map);
 X2dCaptureIdentity decoded={
  .frames=FN(api,B_SNAPSHOT_FRAMES,uint32_t(*)(const void*))(snapshot),
  .sequence=FN(api,B_SNAPSHOT_SEQUENCE,uint32_t(*)(const void*))(snapshot),
  .drive_mode=(uint8_t)FN(api,B_SNAPSHOT_DRIVE,uint32_t(*)(const void*))(snapshot),
  .image_format=(uint8_t)FN(api,B_SNAPSHOT_FORMAT,uint32_t(*)(const void*))(snapshot)};
 FN(api,B_SNAPSHOT_DELETE,void(*)(void*))(snapshot);
 *identity=decoded;return 1;
}
