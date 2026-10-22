/* 固定官方 4.2.0 的真实拍摄首写试验。只有已准备的本任务位移组分流，
 * 直接交接原厂图像和完整 metadata；普通拍摄调用原函数。
 * 试验只保存 multishot 子目录，尚未连接菜单或最终成片发布。 */
#include "factory_private_frame.h"
#include "private_capture_group.h"
#include "private_capture_raw_profile.h"
#include "storage_private_bindings.inc"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
struct dl_phdr_info {uintptr_t base;const char *name;const void *phdr;unsigned short phnum;};
extern int dl_iterate_phdr(int(*)(struct dl_phdr_info*,size_t,void*),void*);
typedef struct {uintptr_t function;void *result;uintptr_t args[8];} NativeCall;
typedef struct {void *allocation,*data;intptr_t length;} RawView;
extern void x2d_qt_native_call(const NativeCall *);
#ifndef X2D_PRIVATE_CAPTURE_JOB
#error Exclusive private capture directory required
#endif
#if defined(X2D_MEMORY_CAPTURE_SESSION) || defined(X2D_DISK_CAPTURE_SESSION)
#include "capture_runtime_job.h"
static char memory_job[108];
#undef X2D_PRIVATE_CAPTURE_JOB
#define X2D_PRIVATE_CAPTURE_JOB memory_job
#endif
#if X2D_PRIVATE_CAPTURE_FRAMES != 4 && X2D_PRIVATE_CAPTURE_FRAMES != 6 && X2D_PRIVATE_CAPTURE_FRAMES != 16
#error Only reviewed four, six and sixteen shot groups are allowed
#endif
#if X2D_PRIVATE_CAPTURE_FRAMES == 16 && defined(X2D_MEMORY_CAPTURE_SESSION)
#error Sixteen-shot uses private disk capture, never the six-shot memory route
#endif
#define FN(offset,type) ((type)(base+(offset)))
typedef struct {
 X2dPrivateFrame frame;
 X2dQtShared original_raw;
 RawView pixels;
 unsigned index,verified,released;
 char path[256];
#ifdef X2D_MEMORY_CAPTURE_SESSION
 unsigned memory_accepted_receipts;
#endif
} CaptureFrame;
static uintptr_t base;
static X2dPrivateNativeApi api;
static X2dCaptureGroup group;
static CaptureFrame frames[X2D_PRIVATE_CAPTURE_FRAMES];
static void *original_cache;
static int log_fd=-1,ready,failed,all_written;
static unsigned ambiguous;
static int aborting,abort_done;
static uint8_t raw_header[16384];
#if X2D_PRIVATE_CAPTURE_FRAMES != 16
static uint8_t compare_buffer[65536];
#endif
static void log_line(const char *line){
 if(log_fd>=0){(void)write(log_fd,line,strlen(line));(void)fsync(log_fd);}
}
static int owned_marker(const char *name){
 char path[256];int n=snprintf(path,sizeof path,"%s/%s",X2D_PRIVATE_CAPTURE_JOB,name);
 if(n<=0||(size_t)n>=sizeof path)return 0;
 int fd=open(path,O_CREAT|O_EXCL|O_NOFOLLOW|O_WRONLY,0600);
 if(fd<0)return 0;int ok=fsync(fd)==0;int closed=close(fd)==0;return ok&&closed;
}
static void failure(const char *message){
 if(!failed){failed=1;group.failed=1;log_line(message);(void)owned_marker("failed");}
}
static uint16_t u16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|(uint32_t)u16(p+2)<<16;}
static const uint8_t *tag(uint32_t ifd,unsigned key){
 if(ifd>sizeof raw_header-2)return 0;
 unsigned count=u16(raw_header+ifd);
 if(count>256||(uint64_t)ifd+2+12*count+4>sizeof raw_header)return 0;
 const uint8_t *found=0;
 for(unsigned i=0;i<count;i++){
  const uint8_t *entry=raw_header+ifd+2+12*i;
  if(u16(entry)==key){if(found)return 0;found=entry;}
 }
 return found;
}
static int scalar(uint32_t ifd,unsigned key,uint32_t *value){
 const uint8_t *entry=tag(ifd,key);
 if(!entry||u32(entry+4)!=1)return 0;
 if(u16(entry+2)==3)*value=u16(entry+8);
 else if(u16(entry+2)==4)*value=u32(entry+8);
 else return 0;
 return 1;
}
static int read_exact(int fd,void *buffer,size_t bytes){
 uint8_t *next=buffer;
 while(bytes){ssize_t n=read(fd,next,bytes);if(n<=0)return 0;next+=n;bytes-=(size_t)n;}
 return 1;
}
static int destination_ready(void *context,const char *path){
 CaptureFrame *frame=context;
 if(!frame||strcmp(path,frame->path))return 0;
 int fd=open(X2D_PRIVATE_CAPTURE_JOB,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
 if(fd<0)return 0;close(fd);return access(path,F_OK)!=0;
}
static int verify_raw(void *context,const char *path){
 CaptureFrame *frame=context;
 if(!frame||strcmp(path,frame->path)||!frame->pixels.data||frame->pixels.length<210510336)return 0;
 int fd=open(path,O_RDONLY|O_NOFOLLOW);if(fd<0)return 0;
 uint32_t main,raw_ifd,width,height,bits,compression,offset,length,maker,exif;
 int ok=read_exact(fd,raw_header,sizeof raw_header)&&!memcmp(raw_header,"II\052\000",4);
 main=ok?u32(raw_header+4):0;
 ok=ok&&scalar(main,330,&raw_ifd)&&scalar(raw_ifd,256,&width)&&width==11904&&
  scalar(raw_ifd,257,&height)&&height==8842&&scalar(raw_ifd,258,&bits)&&bits==16&&
  scalar(raw_ifd,259,&compression)&&compression==1&&scalar(raw_ifd,273,&offset)&&
  offset>=sizeof raw_header&&scalar(raw_ifd,279,&length)&&length==210510336;
 /* 原厂 MakerNote tag 33 的组数与顺序必须与真实快照一致。 */
 const uint8_t *maker_entry=ok?tag(raw_ifd,37500):0;
 if(!maker_entry)maker_entry=ok?tag(main,37500):0;
 if(!maker_entry&&ok&&scalar(main,34665,&exif))maker_entry=tag(exif,37500);
 if(!maker_entry)ok=0;
 maker=ok?u32(maker_entry+8):0;
 const uint8_t *group_tag=ok?tag(maker,33):0;
 if(!group_tag||u16(group_tag+2)!=9||u32(group_tag+4)!=4)ok=0;
 unsigned group_offset=ok?u32(group_tag+8):0;
 if(group_offset>sizeof raw_header-16||!ok||
    u32(raw_header+group_offset)!=group.frames||u32(raw_header+group_offset+4)!=frame->index)ok=0;
 off_t file_bytes=lseek(fd,0,SEEK_END);
 if(file_bytes<=0||!ok||(uint64_t)offset+length>(uint64_t)file_bytes||
    lseek(fd,offset,SEEK_SET)!=(off_t)offset)ok=0;
#if X2D_PRIVATE_CAPTURE_FRAMES != 16
 size_t remaining=ok?length:0,at=0;
 while(remaining){
  size_t bytes=remaining<sizeof compare_buffer?remaining:sizeof compare_buffer;
  if(!read_exact(fd,compare_buffer,bytes)||memcmp(compare_buffer,(uint8_t*)frame->pixels.data+at,bytes)){ok=0;break;}
  at+=bytes;remaining-=bytes;
 }
#endif
 if(close(fd))ok=0;
 if(ok){frame->verified=1;
#if X2D_PRIVATE_CAPTURE_FRAMES == 16
  log_line("PRIVATE_CAPTURE_RAW_HEADER_RANGE_AND_GROUP_VERIFIED\n");
#else
  log_line("PRIVATE_CAPTURE_ALL_RAW_SAMPLES_AND_GROUP_VERIFIED\n");
#endif
 }
 else log_line("PRIVATE_CAPTURE_STORED_RAW_REJECTED\n");
 return ok;
}
static void frame_changed(void *context,X2dPrivateFrame *frame){
 (void)context;
 if(frame->status==X2D_FRAME_FAILED){
  const char *reason="PRIVATE_CAPTURE_CALLBACK_REASON_UNREVIEWED\n";
  switch(frame->failure_reason){
  case X2D_FRAME_FAILURE_MEMORY_ROUTE:reason="PRIVATE_CAPTURE_CALLBACK_MEMORY_ROUTE_CANCELLED\n";break;
  case X2D_FRAME_FAILURE_THREAD:reason="PRIVATE_CAPTURE_CALLBACK_THREAD_REJECTED\n";break;
  case X2D_FRAME_FAILURE_FLUSH_STATE:reason="PRIVATE_CAPTURE_CALLBACK_FLUSH_STATE_REJECTED\n";break;
  case X2D_FRAME_FAILURE_DUPLICATE_FLUSH:reason="PRIVATE_CAPTURE_CALLBACK_DUPLICATE_FLUSH_REJECTED\n";break;
  case X2D_FRAME_FAILURE_WORK_SHARED:reason="PRIVATE_CAPTURE_CALLBACK_WORK_SHARED_REJECTED\n";break;
  case X2D_FRAME_FAILURE_WORK_TYPE:reason="PRIVATE_CAPTURE_CALLBACK_WORK_TYPE_REJECTED\n";break;
  case X2D_FRAME_FAILURE_WORK_PATH:reason="PRIVATE_CAPTURE_CALLBACK_WORK_PATH_REJECTED\n";break;
  case X2D_FRAME_FAILURE_DESTINATION:reason="PRIVATE_CAPTURE_CALLBACK_DESTINATION_REJECTED\n";break;
  case X2D_FRAME_FAILURE_STORED_RAW:reason="PRIVATE_CAPTURE_CALLBACK_STORED_RAW_REJECTED\n";break;
  }
  log_line(reason);failure("PRIVATE_CAPTURE_NATIVE_WRITE_FAILED\n");
 }
}
#ifndef X2D_MEMORY_CAPTURE_SESSION
static X2dQtShared retain(const X2dQtShared *source){
 if(source->control){
  __atomic_fetch_add((uint32_t*)source->control,1,__ATOMIC_RELAXED);
  __atomic_fetch_add((uint32_t*)source->control+1,1,__ATOMIC_RELAXED);
 }
 return *source;
}
static int hold_raw(CaptureFrame *frame,const X2dQtShared *image){
 if(!image||!image->value||!image->control){log_line("PRIVATE_CAPTURE_RAW_SHARED_IMAGE_MISSING\n");return 0;}
 if(frame->original_raw.control){log_line("PRIVATE_CAPTURE_RAW_DUPLICATE_REJECTED\n");return 0;}
 const uint8_t *object=image->value;uint32_t width,height;
 memcpy(&width,object+0x70,4);memcpy(&height,object+0x74,4);
 uint32_t row_bytes=FN(B_IMAGE_STRIDE,uint32_t(*)(const void*))(image->value);
 unsigned reason=x2d_capture_raw_geometry(width,height,object[0x7c],row_bytes);
 char diagnostic[192];
 /* 仅记录尺寸、格式、长度与布尔状态；不写指针、曝光编号、元数据或照片内容。 */
 int n=snprintf(diagnostic,sizeof diagnostic,
  "PRIVATE_CAPTURE_RAW_GEOMETRY width=%u height=%u format=%u row_bytes=%u reason=%u\n",
  width,height,(unsigned)object[0x7c],row_bytes,reason);
 if(n>0&&(size_t)n<sizeof diagnostic)log_line(diagnostic);
 if(reason!=X2D_CAPTURE_RAW_VALID)return 0;
 RawView pixels={0};
 NativeCall view={.function=base+B_IMAGE_RAW_DATA,.result=&pixels,.args={(uintptr_t)image->value}};
 x2d_qt_native_call(&view);
 reason=x2d_capture_raw_view(pixels.allocation!=0,pixels.data!=0,pixels.length);
 n=snprintf(diagnostic,sizeof diagnostic,
  "PRIVATE_CAPTURE_RAW_VIEW length=%lld owns_allocation=%u has_data=%u reason=%u\n",
  (long long)pixels.length,(unsigned)(pixels.allocation!=0),(unsigned)(pixels.data!=0),reason);
 if(n>0&&(size_t)n<sizeof diagnostic)log_line(diagnostic);
 if(reason!=X2D_CAPTURE_RAW_VALID)return 0;
 frame->original_raw=retain(image);frame->pixels=pixels;return 1;
}
#endif
static void reject_notification(const X2dQtShared *notification){
 if(!notification||!notification->value)return;
 X2dQtString message={0};_Alignas(16) uint8_t invalid[32]={0};invalid[24]=2;
 FN(B_NOTIFICATION_SET_RESULT,void(*)(void*,uint32_t,const X2dQtString*,const void*))(
  notification->value,1,&message,invalid); /* 固定枚举 E_ReturnStatus_Error。 */
}
#ifdef X2D_MEMORY_CAPTURE_SESSION
#include "private_memory_session.inc"
#endif
#ifdef X2D_DISK_CAPTURE_SESSION
#include "private_disk_metadata.inc"
#endif
static int receive_owned(unsigned index,const X2dExposureInfo *info,const X2dQtShared *notification){
 CaptureFrame *frame=frames+index;
 if(aborting){failure("SIXTEEN_ABORT_LATE_NOTIFICATION_REJECTED\n");reject_notification(notification);return 1;}
 if(frame->released){
  failure("PRIVATE_CAPTURE_LATE_INPUT_REJECTED\n");reject_notification(notification);return 1;
 }
 if(frame->frame.status==X2D_FRAME_EMPTY){
  X2dPrivateFrameCallbacks callbacks={destination_ready,verify_raw,frame_changed,frame};
  if(!x2d_private_frame_create(&frame->frame,&api,original_cache,info->exposure_id,frame->path,callbacks)){
   failure("PRIVATE_CAPTURE_PRIVATE_WRITE_CREATION_FAILED\n");reject_notification(notification);return 1;
  }
#ifdef X2D_DISK_CAPTURE_SESSION
  if(!x2d_private_frame_observe_work(&frame->frame,disk_observe_work,frame)){
   failure("SIXTEEN_NATIVE_METADATA_OBSERVER_REJECTED\n");reject_notification(notification);return 1;
  }
#endif
  log_line("PRIVATE_CAPTURE_ACTUAL_EXPOSURE_PRIVATE_WRITER_CREATED\n");
#ifdef X2D_MEMORY_CAPTURE_SESSION
  if(!x2d_private_frame_route_memory(&frame->frame,memory_claim_work,frame)){
   failure("MEMORY_RAW_ROUTE_REGISTRATION_FAILED\n");reject_notification(notification);return 1;
  }
#endif
 }
#ifdef X2D_MEMORY_CAPTURE_SESSION
 memory_receive(frame,info);
 if(memory_failed){reject_notification(notification);return 1;}
 if(!memory_accept_notification(frame,info,notification)){
  failure("MEMORY_NOTIFICATION_ACCEPT_FAILED\n");memory_failure();reject_notification(notification);
 }
#else
#ifdef X2D_DISK_CAPTURE_SESSION
 if(!disk_metadata_receive(frame,info)){
  failure("SIXTEEN_NATIVE_METADATA_SOURCE_REJECTED\n");reject_notification(notification);return 1;
 }
#endif
 if(info->kind==1&&!hold_raw(frame,&info->image)){
  failure("PRIVATE_CAPTURE_RAW_INPUT_REJECTED\n");reject_notification(notification);return 1;
 }
 if(!x2d_private_frame_receive(&frame->frame,info,notification)){
  failure("PRIVATE_CAPTURE_FRAME_RECEIVE_REJECTED\n");reject_notification(notification);return 1;
 }
#endif
 log_line("PRIVATE_CAPTURE_GENUINE_NOTIFICATION_AND_METADATA_FORWARDED\n");return 1;
}
static int receive(void *service,const X2dExposureInfo *info,const X2dQtShared *notification){
 if(!ready||!group.issued)return FN(B_SERVICE_ADD_EXPOSURE,int(*)(void*,const void*,const void*))(
  service,info,notification);
 if(!info||!notification||!notification->value||!notification->control){
  failure("PRIVATE_CAPTURE_INVALID_NOTIFICATION_REJECTED\n");return 0;
 }
 X2dCaptureIdentity identity={0};
 int has_snapshot=x2d_capture_metadata_identity(&api,&info->metadata_map,&identity);
 unsigned index;
 int classification=x2d_capture_group_classify(&group,info->exposure_id,has_snapshot?&identity:0,&index);
 if(classification==X2D_CAPTURE_NORMAL)return FN(B_SERVICE_ADD_EXPOSURE,int(*)(void*,const void*,const void*))(
  service,info,notification);
 if(classification==X2D_CAPTURE_CONFLICT){
  ambiguous++;failure("PRIVATE_CAPTURE_OWNERSHIP_CONFLICT_REJECTED\n");reject_notification(notification);return 1;
 }
 /* 与原厂 addExposureItem 一样维护准备状态；不修改全局 Cache。 */
 FN(B_STORAGE_PREPARING,void(*)(void*,int))(service,1);
 int result=receive_owned(index,info,notification);
 FN(B_STORAGE_PREPARING,void(*)(void*,int))(service,0);
 return result;
}
static void drain_completed(void){
#ifdef X2D_MEMORY_CAPTURE_SESSION
 /* 新分支没有六张文件的 written 位，也不调用文件验证或 fileFlushed。 */
 memory_drain_frames();return;
#endif
 for(unsigned i=0;i<group.frames;i++){
  CaptureFrame *frame=frames+i;
  if(frame->frame.status!=X2D_FRAME_READY||frame->released)continue;
#ifdef X2D_DISK_CAPTURE_SESSION
  if(!disk_metadata_complete(frame)){
   failure("SIXTEEN_NATIVE_METADATA_COMPLETION_FAILED\n");return;
  }
#endif
  if(!frame->verified||!x2d_capture_group_written(&group,i,frame->frame.exposure_id)||
     !x2d_private_frame_release(&frame->frame)){
   failure("PRIVATE_CAPTURE_COMPLETION_REFERENCE_RELEASE_FAILED\n");return;
  }
  FN(B_SHARED_DESTROY,void(*)(X2dQtShared*))(&frame->original_raw);
  memset(&frame->original_raw,0,sizeof frame->original_raw);memset(&frame->pixels,0,sizeof frame->pixels);
  frame->released=1;log_line("PRIVATE_CAPTURE_WRITTEN_FRAME_REFERENCES_RELEASED\n");
#ifdef X2D_MEMORY_CAPTURE_SESSION
  char marker[32];int n=snprintf(marker,sizeof marker,"frame-%u.ready",i);
  if(n<=0||n>=(int)sizeof marker||!owned_marker(marker))failure("MEMORY_FRAME_READY_MARKER_FAILED\n");
#endif
 }
 if(!failed&&!all_written&&group.written==((1u<<group.frames)-1)){
  if(!owned_marker("frames.ready")){failure("PRIVATE_CAPTURE_READY_MARKER_FAILED\n");return;}
  all_written=1;log_line("PRIVATE_CAPTURE_GROUP_FIRST_WRITE_COMPLETE_NO_ALBUM\n");
 }
}
#include "sixteen_abort.inc"
static uint32_t getter(const void *service){
 uint32_t result=FN(B_STORAGE_CAPTURE_MODE,uint32_t(*)(const void*))(service);
 if(!ready){
  if(*(uintptr_t*)service!=base+B_SERVICE_VTABLE+16)return result;
  void *cache=*(void**)((uint8_t*)service+0xe0);
  if(!cache||*(uintptr_t*)cache!=base+B_CACHE_VTABLE+16)return result;
  original_cache=cache;ready=1;
#ifdef X2D_DISK_CAPTURE_SESSION
  if(!disk_prepare((void*)service)){failure("SIXTEEN_NATIVE_METADATA_PREPARE_FAILED\n");return result;}
#endif
#ifdef X2D_MEMORY_CAPTURE_SESSION
  if(!memory_prepare((void*)service)){failure("MEMORY_SESSION_NATIVE_PREPARE_FAILED\n");return result;}
#endif
  if(!owned_marker("hook.ready")){failure("PRIVATE_CAPTURE_HOOK_READY_MARKER_FAILED\n");return result;}
  log_line("PRIVATE_CAPTURE_NATIVE_STORAGE_HOOK_READY\n");
 }
 if(ready&&disk_abort_poll((void*)service))return result;
 if(!group.issued&&!failed){
  char requested[256];
  int size=snprintf(requested,sizeof requested,"%s/capture.requested",X2D_PRIVATE_CAPTURE_JOB);
  int fd=size>0&&size<(int)sizeof requested?open(requested,O_RDONLY|O_NOFOLLOW):-1;
  if(fd>=0){
   char text[12]={0},expected[12];ssize_t n=read(fd,text,sizeof text);close(fd);
   int expected_bytes=snprintf(expected,sizeof expected,"%u\n",X2D_PRIVATE_CAPTURE_FRAMES);
   if(expected_bytes<=0||expected_bytes>=(int)sizeof expected||n!=expected_bytes||memcmp(text,expected,(size_t)expected_bytes)||
      !x2d_capture_group_request_issued(&group,1)||!owned_marker("owner.ready"))
    failure("PRIVATE_CAPTURE_OWN_REQUEST_MARKER_REJECTED\n");
   else {
    log_line("PRIVATE_CAPTURE_OWN_REQUEST_ARMED\n");
#ifdef X2D_MEMORY_CAPTURE_SESSION
    memory_issue();
#endif
   }
  }
 }
 if(ready)drain_completed();
#ifdef X2D_MEMORY_CAPTURE_SESSION
 if(ready)memory_pump();
#endif
 return result;
}
static int image_base(struct dl_phdr_info *image,size_t size,void *out){
 (void)size;
 if(image->name&&(!image->name[0]||!strcmp(image->name,"/system/bin/camera-storage"))){
  *(uintptr_t*)out=image->base;return 1;
 }
 return 0;
}
static int replace_pointer(uintptr_t address,uintptr_t value){
 int fd=open("/proc/self/mem",O_RDWR);if(fd<0)return 0;
 int ok=lseek(fd,(off_t)address,SEEK_SET)==(off_t)address&&write(fd,&value,sizeof value)==sizeof value;
 close(fd);return ok&&*(uintptr_t*)address==value;
}
__attribute__((constructor))static void install(void){
 const char *enabled=getenv("X2D_PRIVATE_CAPTURE_ABI");if(!enabled||strcmp(enabled,"1"))return;
#ifdef X2D_MEMORY_CAPTURE_SESSION
 if(!capture_runtime_job(memory_job,getenv("X2D_MEMORY_CAPTURE_JOB")))return;
#elif defined(X2D_DISK_CAPTURE_SESSION)
 if(!capture_runtime_job(memory_job,getenv("X2D_DISK_CAPTURE_JOB")))return;
#endif
 char path[128];ssize_t n=readlink("/proc/self/exe",path,sizeof path-1);
 if(n<=0||n>=(ssize_t)sizeof path-1)return;path[n]=0;
 if(strcmp(path,"/system/bin/camera-storage"))return;
 int directory=open(X2D_PRIVATE_CAPTURE_JOB,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
 if(directory<0)return;close(directory);
 char log_path[256];int log_length=snprintf(log_path,sizeof log_path,"%s/capture.log",X2D_PRIVATE_CAPTURE_JOB);
 if(log_length<=0||log_length>=(int)sizeof log_path)return;
 log_fd=open(log_path,O_CREAT|O_EXCL|O_APPEND|O_NOFOLLOW|O_WRONLY,0600);
 if(log_fd<0)return;
 dl_iterate_phdr(image_base,&base);
 if(!base||!x2d_private_frame_bind(&api,base)){failure("PRIVATE_CAPTURE_ABI_PROFILE_REJECTED\n");return;}
 uintptr_t table=base+B_SERVICE_VTABLE+16;
 if(*(uintptr_t*)(table+0x80)!=base+B_STORAGE_CAPTURE_MODE||
    *(uintptr_t*)(table+0x178)!=base+B_SERVICE_ADD_EXPOSURE){
  failure("PRIVATE_CAPTURE_SERVICE_PROFILE_REJECTED\n");return;
 }
 for(unsigned i=0;i<X2D_PRIVATE_CAPTURE_FRAMES;i++){
  frames[i].index=i;
  int length=snprintf(frames[i].path,sizeof frames[i].path,"%s/frame-%u.3FR",X2D_PRIVATE_CAPTURE_JOB,i);
  if(length<=0||(size_t)length>=sizeof frames[i].path||access(frames[i].path,F_OK)==0){
   failure("PRIVATE_CAPTURE_DESTINATION_REJECTED\n");return;
  }
 }
 if(!x2d_capture_group_arm(&group,1,X2D_PRIVATE_CAPTURE_FRAMES)){
  failure("PRIVATE_CAPTURE_GROUP_ARM_REJECTED\n");return;
 }
 if(!replace_pointer(table+0x178,(uintptr_t)receive)){
  failure("PRIVATE_CAPTURE_NOTIFY_HOOK_FAILED\n");return;
 }
 if(!replace_pointer(table+0x80,(uintptr_t)getter)){
  (void)replace_pointer(table+0x178,base+B_SERVICE_ADD_EXPOSURE);
  failure("PRIVATE_CAPTURE_GETTER_HOOK_FAILED\n");return;
 }
 log_line("PRIVATE_CAPTURE_VERSION_BOUND_HOOKS_INSTALLED\n");
}
__attribute__((destructor))static void close_log(void){
#ifdef X2D_DISK_CAPTURE_SESSION
 ch_metadata_destroy(&disk_metadata);
 /* 程序退出时不再调用 QObject；只关闭本进程的只读 fd。 */
 if(disk_read.active)close(disk_read.fd);
#endif
 if(log_fd>=0)close(log_fd);
}
