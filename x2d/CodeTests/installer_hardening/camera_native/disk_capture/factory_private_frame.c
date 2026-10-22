/* ABI 地址和函数指纹由固定 4.2.0 离线固件生成，拒绝其他版本。
 * Qt 结构核对来源：qt/qtbase v6.4.1 qobjectdefs_impl.h、qsharedpointer_impl.h。
 * 一次接收试验创建了对象但未完成首写；调用候选不能代替存储完成与联调。
 */
#include "factory_private_frame.h"
#include <string.h>
#ifndef X2D_PRIVATE_FRAME_HOST_CHECK
#include <dlfcn.h>
#else
extern void *x2d_private_mock_symbol(uintptr_t address);
extern void *x2d_private_mock_dlsym(void *handle,const char *name);
#define dlsym x2d_private_mock_dlsym
#endif
#include "storage_private_bindings.inc"

typedef struct { uintptr_t function; void *result; uintptr_t args[8]; } NativeCall;
extern void x2d_qt_native_call(const NativeCall *call);
typedef struct { uint32_t references,padding; void (*impl)(int,void*,void*,void**,uint8_t*);
 X2dPrivateFrame *frame; uint32_t kind,padding2; } NativeSlot;
_Static_assert(offsetof(NativeSlot,impl)==8,"Qt slot function offset differs");
_Static_assert(offsetof(NativeSlot,frame)==16,"Qt slot context offset differs");
_Static_assert(sizeof(NativeSlot)==32,"Qt slot allocation differs");

#ifndef X2D_PRIVATE_FRAME_HOST_CHECK
#define FN(api,offset,type) ((type)((api)->base+(offset)))
#else
#define FN(api,offset,type) ((type)x2d_private_mock_symbol((api)->base+(offset)))
#endif
static void *allocate(X2dPrivateNativeApi *a,size_t n){
 return FN(a,B_OPERATOR_NEW,void*(*)(size_t))(n);
}
static void deallocate(X2dPrivateNativeApi *a,void *p){
 FN(a,B_OPERATOR_DELETE,void(*)(void*))(p);
}
static void shared_release(X2dPrivateNativeApi *a,X2dQtShared *p){
 FN(a,B_SHARED_DESTROY,void(*)(X2dQtShared*))(p);
 p->value=0;p->control=0;
}
static X2dQtShared shared_copy(const X2dQtShared *p){
 if(p->control){
  uint32_t *counts=p->control;
  __atomic_fetch_add(counts,1,__ATOMIC_RELAXED);
  __atomic_fetch_add(counts+1,1,__ATOMIC_RELAXED);
 }
 return *p;
}
static void string_destroy(X2dPrivateNativeApi *a,X2dQtString *s){
 if(s->allocation && __atomic_sub_fetch((int*)s->allocation,1,__ATOMIC_ACQ_REL)==0)
  FN(a,B_ARRAY_DEALLOCATE,void(*)(void*,intptr_t,intptr_t))(s->allocation,2,8);
 memset(s,0,sizeof(*s));
}
static void string_create(X2dPrivateNativeApi *a,X2dQtString *s,const char *text){
 NativeCall c={.function=a->base+B_STRING_UTF8,.result=s,
  .args={strlen(text),(uintptr_t)text}};
 x2d_qt_native_call(&c);
}
static int same_thread(X2dPrivateFrame *f){
 return f->api->qobject_thread(f->storage_io)==f->api->current_thread();
}
static void fail(X2dPrivateFrame *f,unsigned reason){
 if(f->status==X2D_FRAME_FAILED)return;
 f->status=X2D_FRAME_FAILED;f->failure_reason=reason;
 if(f->callbacks.changed)f->callbacks.changed(f->callbacks.context,f);
}
static unsigned own_work_path(X2dPrivateFrame *f,const X2dQtShared *work){
 if(!work||!work->value||!work->control)return X2D_FRAME_FAILURE_WORK_SHARED;
 /* 只转发本对象的 RAW WriteWork，不接受其他文件类型。 */
 if(*((const uint8_t*)work->value+0x38)!=1)return X2D_FRAME_FAILURE_WORK_TYPE;
 X2dQtString path={0};
 NativeCall c={.function=f->api->base+B_WORK_PATH,.result=&path,.args={(uintptr_t)work->value}};
 x2d_qt_native_call(&c);
 size_t n=strlen(f->path);
 int equal=path.size==(intptr_t)n && path.data;
 if(equal)for(size_t i=0;i<n;i++)if(path.data[i]!=(uint8_t)f->path[i]){equal=0;break;}
 string_destroy(f->api,&path);
 return equal?X2D_FRAME_FAILURE_NONE:X2D_FRAME_FAILURE_WORK_PATH;
}
static void slot_impl(int operation,void *self,void *receiver,void **args,uint8_t *result){
 (void)receiver;
 NativeSlot *slot=self;
 X2dPrivateFrame *f=slot->frame;
 if(operation==0){deallocate(f->api,self);return;}
 if(operation==2){if(result)*result=0;return;}
 if(operation!=1||f->status==X2D_FRAME_EMPTY)return;
 /* 取消先发生、元数据后台任务后返回时，仍接住该对象首个合法 RAW
  * Work，使 owner 能在消费者排空后析构；失败分支永不转发 FileIO。 */
 int cancelled=f->status==X2D_FRAME_FAILED;
 if(cancelled&&(!f->memory_raw_work||slot->kind!=0||f->submitted))return;
 if(!same_thread(f)){fail(f,X2D_FRAME_FAILURE_THREAD);return;}
 if(slot->kind==0){
  const X2dQtShared *work=args?args[1]:0;
  if(f->status!=X2D_FRAME_PENDING&&!cancelled){fail(f,X2D_FRAME_FAILURE_FLUSH_STATE);return;}
  if(f->submitted){fail(f,X2D_FRAME_FAILURE_DUPLICATE_FLUSH);return;}
  unsigned reason=own_work_path(f,work);
  if(reason!=X2D_FRAME_FAILURE_NONE){fail(f,reason);return;}
  if(!f->callbacks.destination_ready(f->callbacks.context,f->path)){
   fail(f,X2D_FRAME_FAILURE_DESTINATION);return;
  }
  f->submitted=1;
  if(!cancelled&&f->observe_raw_work)f->observe_raw_work(f->observe_context,f,work);
  if(cancelled)return;
  if(f->memory_raw_work){
   if(!f->memory_raw_work(f->memory_context,f,work))fail(f,X2D_FRAME_FAILURE_MEMORY_ROUTE);
   return;
  }
  FN(f->api,B_FLUSH_ITEM,void(*)(void*,const X2dQtShared*,const X2dQtShared*))(
   f->storage_io,&f->item,work);
 }else{
  if(f->memory_raw_work)return;
  int flushed=FN(f->api,B_ITEM_IS_FLUSHED,int(*)(const void*))(f->item.value);
  if(flushed && f->submitted && f->status==X2D_FRAME_PENDING){
   if(f->callbacks.verify_finished_raw(f->callbacks.context,f->path)){
    f->status=X2D_FRAME_READY;
    if(f->callbacks.changed)f->callbacks.changed(f->callbacks.context,f);
   }else fail(f,X2D_FRAME_FAILURE_STORED_RAW);
  }
 }
}
static int connect_slot(X2dPrivateFrame *f,unsigned index,uintptr_t signal){
 NativeSlot *slot=allocate(f->api,sizeof(*slot));
 if(!slot)return 0;
 *slot=(NativeSlot){.references=1,.impl=slot_impl,.frame=f,.kind=index};
 uintptr_t member[2]={f->api->base+signal,0};
 NativeCall call={.function=f->api->base+B_CONNECT_IMPL,.result=&f->connections[index],
  .args={(uintptr_t)f->item.value,(uintptr_t)member,(uintptr_t)f->storage_io,0,
         (uintptr_t)slot,1,0,f->api->base+B_CACHE_ITEM_META}};
 /* 明确 DirectConnection；回调同时核对 QObject 线程，不跨线程调用 StorageIO。 */
 x2d_qt_native_call(&call);
 return f->connections[index]!=0;
}
static void disconnect_slots(X2dPrivateFrame *f){
 for(unsigned i=0;i<2;i++)if(f->connections[i]){
  f->api->disconnect(&f->connections[i]);
  f->api->connection_destroy(&f->connections[i]);
  f->connections[i]=0;
 }
}
int x2d_private_frame_bind(X2dPrivateNativeApi *api,uintptr_t base){
 if(!api||!base)return 0;
 for(size_t i=0;i<sizeof(private_bindings)/sizeof(*private_bindings);i++)
  if(memcmp((const void*)(base+private_bindings[i].address),private_bindings[i].bytes,16))return 0;
 const char *(*version)(void)=dlsym(0,"qVersion");
 if(!version||strcmp(version(),"6.4.1"))return 0;
 *api=(X2dPrivateNativeApi){.base=base,
  .qobject_thread=dlsym(0,"_ZNK7QObject6threadEv"),
  .current_thread=dlsym(0,"_ZN7QThread13currentThreadEv"),
  .disconnect=dlsym(0,"_ZN7QObject10disconnectERKN11QMetaObject10ConnectionE"),
  .connection_destroy=dlsym(0,"_ZN11QMetaObject10ConnectionD1Ev")};
 return api->qobject_thread&&api->current_thread&&api->disconnect&&api->connection_destroy;
}
#ifndef X2D_PRIVATE_FRAME_PATH_COUNT
#define X2D_PRIVATE_FRAME_PATH_COUNT 6
#endif
#if X2D_PRIVATE_FRAME_PATH_COUNT != 6 && X2D_PRIVATE_FRAME_PATH_COUNT != 16
#error Unreviewed private frame path count
#endif
static int path_valid(const char *path){
 const char *prefix="/mnt/media_rw/ssd/multishot/";
 size_t prefix_n=strlen(prefix),n=path?strlen(path):0;
 if(n>=256||n<=prefix_n||strncmp(path,prefix,prefix_n))return 0;
 const char *slash=strchr(path+prefix_n,'/');
 if(!slash||slash==path+prefix_n||strncmp(slash,"/frame-",7))return 0;
 const char *number=slash+7;
 if(number[0]<'0'||number[0]>'9')return 0;
 unsigned index=(unsigned)(number[0]-'0');
 const char *extension=number+1;
 if(*extension>='0'&&*extension<='9'){
  if(number[0]=='0')return 0;
  index=index*10+(unsigned)(*extension-'0');extension++;
 }
 if(index>=X2D_PRIVATE_FRAME_PATH_COUNT||strcmp(extension,".3FR"))return 0;
 for(const char *p=path+prefix_n;p<slash;p++)
  if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'))return 0;
 return 1;
}
int x2d_private_frame_create(X2dPrivateFrame *f,X2dPrivateNativeApi *a,
 void *cache,uint32_t id,const char *path,X2dPrivateFrameCallbacks callbacks){
 if(!f||f->status!=X2D_FRAME_EMPTY||!a||!a->base||!cache||!path_valid(path)||
    !callbacks.destination_ready||!callbacks.verify_finished_raw||
    !callbacks.destination_ready(callbacks.context,path))return 0;
 if(*(uintptr_t*)cache!=a->base+B_CACHE_VTABLE+16)return 0;
 /* Cache 的 QObject parent 是 StorageIOInterface；不更换任何原厂 Cache 指针。 */
 void *private=*((void**)cache+1);
 void *io=private?*(void**)((uint8_t*)private+0x10):0;
 if(!io||*(uintptr_t*)io!=a->base+B_IO_VTABLE+16||
    a->qobject_thread(io)!=a->current_thread()||a->qobject_thread(cache)!=a->current_thread())return 0;
 memset(f,0,sizeof(*f));f->api=a;f->storage_io=io;f->exposure_id=id;f->callbacks=callbacks;
 memcpy(f->path,path,strlen(path)+1);f->status=X2D_FRAME_PENDING;
 f->content.value=allocate(a,0xe0);
 if(!f->content.value)goto error;
 FN(a,B_CONTENT_CONSTRUCTOR,void(*)(void*,uint32_t,const void*,void*))(
  f->content.value,id,(uint8_t*)cache+0x58,0);
 f->content.control=allocate(a,24);
 if(!f->content.control){
  FN(a,B_CONTENT_DELETE,void(*)(void*))(f->content.value);f->content.value=0;goto error;
 }
 uint32_t *counts=f->content.control;counts[0]=1;counts[1]=1;
 ((uintptr_t*)f->content.control)[1]=a->base+B_CONTENT_SHARED_DELETE;
 ((void**)f->content.control)[2]=f->content.value;
 FN(a,B_SET_QOBJECT_SHARED,void(*)(void*,void*,int))(f->content.control,f->content.value,1);
 uint8_t raw_format=0,backup=0;
 NativeCall create={.function=a->base+B_CREATE_WRITE_ITEM,.result=&f->item,
  .args={(uintptr_t)&f->content,(uintptr_t)&raw_format,(uintptr_t)&backup}};
 x2d_qt_native_call(&create);
 if(!f->item.value||!f->item.control||!connect_slot(f,0,B_SIGNAL_FLUSH)||!connect_slot(f,1,B_SIGNAL_CHANGED))goto error;
 /* 4.2.0 CacheItemInfo RAW 构造函数（0xa5208）自行追加 .3FR。
  * path_valid 已限定完整文件名；传入 stem，保留完整路径的 Work 校验。 */
 char stem[256];size_t stem_size=strlen(path)-4;
 memcpy(stem,path,stem_size);stem[stem_size]=0;
 X2dQtString filename={0};string_create(a,&filename,stem);
 uint8_t info[40]={0};
 FN(a,B_INFO_CONSTRUCTOR,void(*)(void*,const X2dQtString*,uint32_t))(info,&filename,1);
 FN(a,B_ITEM_ADD_FILE,void(*)(void*,const void*,int))(f->item.value,info,0);
 string_destroy(a,(X2dQtString*)(info+16));
 FN(a,B_FILEINFO_DESTROY,void(*)(void*))(info+8);
 string_destroy(a,&filename);
 return 1;
error:
 disconnect_slots(f);shared_release(a,&f->item);shared_release(a,&f->content);
 memset(f,0,sizeof(*f));return 0;
}
int x2d_private_frame_receive(X2dPrivateFrame *f,const X2dExposureInfo *info,
 const X2dQtShared *notification){
 if(!f||!info||!notification||info->exposure_id!=f->exposure_id ||
    f->status!=X2D_FRAME_PENDING||!same_thread(f))return 0;
 X2dQtString empty={0};
 if(info->kind==0){
  if(f->preview_received)return 0;
  X2dQtShared copy=shared_copy(&info->image);
  FN(f->api,B_INCOMING_PREVIEW,void(*)(void*,X2dQtShared*,const void*,const X2dQtShared*,const X2dQtString*))(
   f->content.value,&copy,&info->metadata_map,notification,&empty);
  shared_release(f->api,&copy);f->preview_received=1;
 }else if(info->kind>=1&&info->kind<=3){
  if(f->image_received)return 0;
  FN(f->api,B_INCOMING_IMAGE,void(*)(void*,const X2dQtShared*,const void*,const X2dQtShared*,const X2dQtString*))(
   f->content.value,&info->image,&info->metadata_map,notification,&empty);
  f->image_received=1;
 }else if(info->kind==255){
  X2dQtShared copy=shared_copy(notification);
  FN(f->api,B_ITEM_SET_METADATA,void(*)(void*,const void*,X2dQtShared*))(
   f->item.value,&info->metadata_map,&copy);
  shared_release(f->api,&copy);f->metadata_received=1;
 }else return 0;
 return 1;
}
int x2d_private_frame_observe_work(X2dPrivateFrame *f,
 void (*observer)(void *,X2dPrivateFrame *,const X2dQtShared *),void *context){
 if(!f||!observer||f->status!=X2D_FRAME_PENDING||!same_thread(f)||
    f->observe_raw_work||f->submitted||f->preview_received||f->image_received||
    f->metadata_received)return 0;
 f->observe_raw_work=observer;f->observe_context=context;return 1;
}
int x2d_private_frame_release(X2dPrivateFrame *f){
 if(!f||f->status==X2D_FRAME_EMPTY)return 1;
 if(!same_thread(f))return 0;
 if(f->memory_raw_work){
  if(!f->memory_finished && (f->image_received||f->preview_received||f->metadata_received||f->submitted))return 0;
 }else{
  int flushed=FN(f->api,B_ITEM_IS_FLUSHED,int(*)(const void*))(f->item.value);
  if(!flushed && (f->image_received||f->preview_received||f->metadata_received||f->submitted))return 0;
 }
 disconnect_slots(f);shared_release(f->api,&f->item);shared_release(f->api,&f->content);
 memset(f,0,sizeof(*f));return 1;
}
int x2d_private_frame_route_memory(X2dPrivateFrame *f,
 int (*route)(void *,X2dPrivateFrame *,const X2dQtShared *),void *context){
 if(!f||!route||f->status!=X2D_FRAME_PENDING||!same_thread(f)||
    f->memory_raw_work||f->submitted||f->preview_received||f->image_received||
    f->metadata_received)return 0;
 f->memory_raw_work=route;f->memory_context=context;return 1;
}
int x2d_private_frame_finish_memory(X2dPrivateFrame *f,int success){
 if(!f||!f->memory_raw_work||f->memory_finished||!same_thread(f)||!f->submitted||
    (success!=0&&success!=1)||
    (success&&f->status!=X2D_FRAME_PENDING))return 0;
 f->memory_finished=1;
 if(success)f->status=X2D_FRAME_MEMORY_DRAINED;
 else fail(f,X2D_FRAME_FAILURE_MEMORY_ROUTE);
 return 1;
}
