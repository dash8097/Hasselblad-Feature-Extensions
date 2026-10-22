#include "capture_metadata_native.h"
#include "storage_private_bindings.inc"
#include <stdlib.h>
#include <string.h>
#include "capture_rawpb_metadata.h"
typedef struct {uintptr_t function;void *result;uintptr_t args[8];} MetadataCall;
typedef struct {void *allocation;const uint8_t *data;intptr_t size;} MetadataBytes;
extern void x2d_qt_native_call(const MetadataCall *);
#ifndef X2D_CAPTURE_METADATA_HOST_CHECK
#define MD_FN(n,offset,type) ((type)((n)->api.base+(offset)))
#else
extern void *ch_metadata_mock_symbol(uintptr_t);
#define MD_FN(n,offset,type) ((type)ch_metadata_mock_symbol((n)->api.base+(offset)))
#endif
static int owner(CmNative *n,ChReadApi read){
 return n&&n->api.base&&n->storage_owner&&n->api.qobject_thread&&n->api.current_thread&&
  n->api.qobject_thread(n->storage_owner)==n->api.current_thread()&&
  read.read&&read.allowed&&read.allowed(read.context);
}
static void release_array(CmNative *n,void *allocation,int width){
 if(allocation&&__atomic_sub_fetch((int*)allocation,1,__ATOMIC_ACQ_REL)==0)
  MD_FN(n,B_ARRAY_DEALLOCATE,void(*)(void*,intptr_t,intptr_t))(allocation,width,8);
}
int ch_metadata_copy(CmNative *n,ChReadApi read,const void *map,ChMetadata *out){
 if(!out||out->bytes||out->size||!map||!owner(n,read))return 0;
 X2dQtString key={0};MetadataBytes source={0};
 _Alignas(16) uint8_t variant[32]={0},invalid[32]={0};invalid[24]=2;
 const char *name="DCAM_CONTAINER";
 MetadataCall string={n->api.base+B_STRING_UTF8,&key,{strlen(name),(uintptr_t)name}};
 x2d_qt_native_call(&string);
 if(!key.data||key.size!=(intptr_t)strlen(name)){release_array(n,key.allocation,2);return 0;}
 MetadataCall value={n->api.base+B_MAP_VALUE,variant,{(uintptr_t)map,(uintptr_t)&key,(uintptr_t)invalid}};
 x2d_qt_native_call(&value);
 release_array(n,key.allocation,2);
 MetadataCall bytes={n->api.base+B_VARIANT_BYTE_ARRAY,&source,{(uintptr_t)variant}};
 x2d_qt_native_call(&bytes);
 MD_FN(n,B_VARIANT_DESTROY,void(*)(void*))(variant);
 int ok=source.data&&source.size>0&&(uint64_t)source.size<=CH_METADATA_MAX;
 uint8_t *copy=ok?malloc((size_t)source.size):NULL;
 if(!copy)ok=0;
 for(size_t done=0;ok&&done<(size_t)source.size;){
  size_t count=(size_t)source.size-done;if(count>16384)count=16384;
  uintptr_t address=(uintptr_t)source.data;
  if(done>UINTPTR_MAX-address||!read.read(read.context,address+done,copy+done,count))ok=0;
  else done+=count;
 }
 release_array(n,source.allocation,1);
 if(!ok){free(copy);return 0;}
 *out=(ChMetadata){copy,(size_t)source.size};return 1;
}
void ch_metadata_destroy(ChMetadata *m){
 if(m){free(m->bytes);memset(m,0,sizeof *m);}
}
static int metadata_number(CmNative *n,const void *map,uintptr_t tag,int sign,uint32_t *out){
 const void *key=MD_FN(n,tag,const void*(*)(void))();
 _Alignas(16) uint8_t value[32]={0},invalid[32]={0};invalid[24]=2;
 if(!key)return 0;
 MetadataCall get={n->api.base+B_MAP_VALUE,value,{(uintptr_t)map,(uintptr_t)key,(uintptr_t)invalid}};
 x2d_qt_native_call(&get);
 unsigned char valid=0;
 uint32_t number=sign?(uint32_t)MD_FN(n,B_VARIANT_INT,int(*)(const void*,unsigned char*))(value,&valid):
  MD_FN(n,B_VARIANT_UINT,uint32_t(*)(const void*,unsigned char*))(value,&valid);
 MD_FN(n,B_VARIANT_DESTROY,void(*)(void*))(value);
 if(!valid||(sign&&number>INT32_MAX))return 0;
 *out=number;return 1;
}
int ch_metadata_rawpb_wb(CmNative *n,ChReadApi read,const void *map,ChMetadata *m){
 if(!map||!m||!owner(n,read)||!crm_layout(m->bytes,m->size))return 0;
 const uintptr_t tags[]={B_TAG_R_DEN,B_TAG_R_NUM,B_TAG_G_DEN,B_TAG_G_NUM,B_TAG_B_DEN,B_TAG_B_NUM,
  B_TAG_R_NEUTRAL,B_TAG_G_NEUTRAL,B_TAG_B_NEUTRAL,B_TAG_NEUTRAL_PRECISION};
 uint32_t values[10]={0};
 for(unsigned i=0;i<10;i++)if(!metadata_number(n,map,tags[i],i<6||i==9,values+i))return 0;
 for(unsigned i=0;i<9;i++)if(!values[i])return 0;
 if(values[9]>30)return 0;
 uint32_t neutral[4]={values[6],values[7],values[8],1u<<values[9]};
 void *container=MD_FN(n,B_DCAM_ALLOC,void*(*)(void))();if(!container)return 0;
 int ok=!MD_FN(n,B_DCAM_DESERIALIZE,int(*)(void*,const void*,unsigned))(container,m->bytes,(unsigned)m->size);
 if(ok){
  MD_FN(n,B_DCAM_SET_BYTES,void(*)(void*,unsigned,const void*,unsigned))(container,0x4d5,values,24);
  MD_FN(n,B_DCAM_SET_BYTES,void(*)(void*,unsigned,const void*,unsigned))(container,0x4d3,neutral,16);
 }
 unsigned size=ok?MD_FN(n,B_DCAM_SERIALIZE_SIZE,unsigned(*)(void*))(container):0;
 uint8_t *copy=size&&size<=CH_METADATA_MAX?malloc(size):NULL;
 if(!copy)ok=0;
 if(ok){MD_FN(n,B_DCAM_SERIALIZE,void(*)(void*,void*,unsigned))(container,copy,size);ok=crm_rawpb_wb_valid(copy,size);}
 MD_FN(n,B_DCAM_FREE,void(*)(void*))(container);
 if(!ok){free(copy);return 0;}
 free(m->bytes);*m=(ChMetadata){copy,size};return 1;
}
int ch_metadata_write_rawpb(CmNative *n,ChReadApi read,const void *content,ChMetadata *m){
 uintptr_t address=(uintptr_t)content,full=0;
 if(!owner(n,read)||!content||address>UINTPTR_MAX-0xd8||
    !read.read(read.context,address+0xd8,&full,sizeof full)||!full)return 0;
 /* 官方 4.2.0 WriteFileContent::toMap 优先读 +0xd8 的 Metadata。
  * 必须先确认该指针，避免缺失时转入基类按文件路径建立缓存。
  * 调用和检查都在持有 content 强引用的 owner 线程执行。 */
 void *map=NULL;X2dQtString empty={0};
 MetadataCall get={n->api.base+B_WRITE_FILE_TO_MAP,&map,{address,(uintptr_t)&empty,1,8}};
 x2d_qt_native_call(&get);
 int ok=map&&ch_metadata_rawpb_wb(n,read,&map,m);
 if(map)MD_FN(n,B_MAP_DESTROY,void(*)(void*))(&map);
 return ok;
}
