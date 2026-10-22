/* 官方 X2D 100C 4.2.0 独占 RAW 堆栈存储适配。
 * 普通路径原样转发；只有已准备任务、逐次放行的普通曝光进入内存。
 * 本库不发曝光，不登记相册。真实拍摄入口由外层新手动请求放行。 */
#define _GNU_SOURCE
#include "factory_private_frame.h"
#include "capture_memory_native.h"
#include "capture_header_native.h"
#include "capture_metadata_native.h"
#include "capture_runtime_job.h"
#include "capture_ion_reference.h"
#include "raw_stack_parallel.h"
#include "raw_stack_async.h"
#include "raw_stack_file.h"
#include "raw_stack_iso.h"
#include "raw_stack_pool.h"
#include "raw_stack_memory_budget.h"
#include "storage_private_bindings.inc"
#include <dlfcn.h>
#include <link.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
typedef struct {uintptr_t function;void *result;uintptr_t args[8];} NativeCall;
typedef struct {void *allocation,*data;intptr_t length;} RawView;
extern void x2d_qt_native_call(const NativeCall *);
static uintptr_t base;
#define FN(offset,type) ((type)(base+(offset)))
static char job[108],frame_path[256];
static X2dPrivateNativeApi api;
static CmNative owner;
static CmApi input;
static ChNative reader;
static ChReadApi read_api;
static ChMetadata metadata;
static X2dPrivateFrame frame;
static X2dQtShared held;
static CmView view;
static RsCore core;
static void *original_cache,*ion,*accumulator[RS_MAX_SEGMENTS],*mapped[RS_MAX_SEGMENTS];
static int (*mem_free)(void *),(*mem_unmap)(void *),(*device_close)(void *);
static RsPoolApi pool_api;
static int pool_owned;
static void *pool_module;
static RsConfig pending_config;
static size_t accumulator_bytes;
static int (*mem_allocate)(void*,void**,uint32_t,uint32_t,uint32_t,uint32_t);
static int (*mem_size)(void*,uint32_t*),(*mem_map)(void*,void**);
static int log_fd=-1,ready,failed,output_done;
static uint64_t requested,completed;
static RsAsync accumulating;
static X2dQtShared accumulating_held,waiting_held;
static CmView waiting_view;
static uint64_t waiting_sequence;
static uint64_t accumulating_sequence,accumulating_started,frame_received_at;
static uint32_t last_exposure,previous_exposure;
static unsigned have_last,claimed,work_ready,first_ready;
static uint8_t first_header[16384];
static uint32_t iso_target,iso_planned;
static RsIsoExposure iso_source;
static uint8_t *preview;
static size_t preview_bytes;
static uint64_t now_ms(void){struct timespec t;return clock_gettime(CLOCK_MONOTONIC,&t)?0:(uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int parse_uint(const char **p,uint64_t *value,char delimiter){
 const char *s=*p;uint64_t n=0;if(*s<'0'||*s>'9'||(*s=='0'&&s[1]>='0'&&s[1]<='9'))return 0;
 do{unsigned d=(unsigned)(*s++-'0');if(n>(UINT64_MAX-d)/10)return 0;n=n*10+d;}while(*s>='0'&&*s<='9');
 if(*s++!=delimiter)return 0;*p=s;*value=n;return 1;
}
static int path(char *out,size_t cap,const char *name){int n=snprintf(out,cap,"%s/%s",job,name);return n>0&&(size_t)n<cap;}
static int put(const char *name,const char *data,int exclusive){
 char p[256],temporary[260];if(!path(p,sizeof p,name))return 0;
 if(!exclusive&&snprintf(temporary,sizeof temporary,"%s.tmp",p)>=(int)sizeof temporary)return 0;
 const char *target=exclusive?p:temporary;
 int fd=open(target,O_WRONLY|O_CREAT|O_NOFOLLOW|O_EXCL,0600);if(fd<0)return 0;
 size_t n=strlen(data);int ok=write(fd,data,n)==(ssize_t)n;if(exclusive&&fsync(fd))ok=0;
 if(close(fd))ok=0;
 if(!exclusive){if(ok&&rename(temporary,p))ok=0;if(!ok)(void)unlink(temporary);}
 return ok;
}
static void log_line(const char *line){if(log_fd>=0)(void)write(log_fd,line,strlen(line));}
static void failure(const char *reason){if(!failed){failed=1;log_line(reason);(void)put("failed","RAW_STACK_FAILED\n",1);}}
static int text_read(const char *name,char *out,size_t bytes){
 char p[256];if(!path(p,sizeof p,name))return -1;int fd=open(p,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return -1;
 struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||(uint64_t)st.st_size>=bytes){close(fd);return -1;}
 ssize_t n=read(fd,out,bytes-1);close(fd);if(n!=st.st_size)return -1;out[n]=0;return (int)n;
}
static int snapshot_budget(size_t bytes){
 char info[4096];int fd=open("/proc/meminfo",O_RDONLY|O_NOFOLLOW);if(fd<0)return 0;
 ssize_t n=read(fd,info,sizeof info-1);close(fd);if(n<=0)return 0;info[n]=0;
 return rs_snapshot_budget(info,bytes);
}
static int pool_prepare(RsConfig config){
 int (*open_device)(const char*,void*,void**)=dlsym(RTLD_DEFAULT,"duss_hal_device_open");
 int (*state)(void*,uint8_t*)=dlsym(RTLD_DEFAULT,"duss_hal_device_get_state");
 mem_allocate=dlsym(RTLD_DEFAULT,"duss_hal_mem_alloc");
 mem_size=dlsym(RTLD_DEFAULT,"duss_hal_mem_get_size");
 mem_map=dlsym(RTLD_DEFAULT,"duss_hal_mem_map");
 mem_free=dlsym(RTLD_DEFAULT,"duss_hal_mem_free");mem_unmap=dlsym(RTLD_DEFAULT,"duss_hal_mem_unmap");
 device_close=dlsym(RTLD_DEFAULT,"duss_hal_device_close");
 pool_api=(RsPoolApi){open_device,state,dlsym(RTLD_DEFAULT,"duss_hal_device_start"),dlsym(RTLD_DEFAULT,"duss_hal_device_stop"),device_close,
  dlsym(RTLD_DEFAULT,"duss_hal_attach_ion_mem"),dlsym(RTLD_DEFAULT,"duss_hal_detach_ion_mem")};
 size_t bytes;
 if(!mem_allocate||!mem_size||!mem_map||!mem_free||!mem_unmap||!device_close||!rs_workspace_bytes(config,&bytes)||bytes>UINT32_MAX){log_line("RAW_STACK_POOL_API_OR_SIZE_REJECTED\n");return 0;}
 const char increment[]="78e547b1d03185f787e42aa7d41019f335ef72e1c3b2070fdf7865225a5c66eb";
 const char decrement[]="f2091f94d8d66e599fe100be7783e211b2a22f2e7083d526e9d01206a50b9322";
 if(!open_device||!public_code_digest((const uint8_t*)(uintptr_t)open_device+0x2a4,20,increment)||
    !public_code_digest((const uint8_t*)(uintptr_t)device_close+0x228,12,decrement)){log_line("RAW_STACK_POOL_ABI_REJECTED\n");return 0;}
 int open_rc=0,ok=rs_pool_acquire(pool_api,&ion,&pool_module,&pool_owned,&open_rc);
 char report[128];snprintf(report,sizeof report,"RAW_STACK_POOL_OPEN rc=%d owned=%d bytes=%llu\n",open_rc,pool_owned,(unsigned long long)bytes);log_line(report);
 if(!ok){log_line("RAW_STACK_POOL_START_OR_STATE_REJECTED\n");return 0;}
 pending_config=config;accumulator_bytes=bytes;
 log_line("RAW_STACK_ACCUMULATOR_DEFERRED_UNTIL_MANUAL_FRAME_REQUEST\n");
 return 1;
}
/* 只有外层暂停取景并收到新手动快门后，frame.request 才会出现。
 * 大缓冲必须在 armed 前就绪；准备模式不占用取景的图像内存。 */
static int pool_allocate(void){
 if(core.workspace)return !failed&&accumulating.pool;
 if(!ion||!accumulator_bytes)return 0;
 for(unsigned i=0;i<RS_MAX_SEGMENTS;i++)if(accumulator[i]||mapped[i])return 0;
 if(!snapshot_budget((size_t)pending_config.height*pending_config.stride_samples*2)){
  log_line("RAW_STACK_NORMAL_MEMORY_SNAPSHOT_BUDGET_REJECTED\n");return 0;
 }
 char report[160];RsSegment segments[RS_MAX_SEGMENTS];
 unsigned count=pending_config.height<RS_MAX_SEGMENTS?pending_config.height:RS_MAX_SEGMENTS;
 /* CPU 累加区请求 heap 2；原厂 HAL 可按其掩码选择后备池。 */
 for(unsigned i=0;i<count;i++){
  RsConfig part=pending_config;size_t bytes=0;uint32_t actual=0;
  part.height=(uint64_t)pending_config.height*(i+1)/count-(uint64_t)pending_config.height*i/count;
  if(!rs_workspace_bytes(part,&bytes)||bytes>UINT32_MAX)return 0;
  int allocation_rc=mem_allocate(ion,accumulator+i,(uint32_t)bytes,1024,2,0x10);
  if(allocation_rc||!accumulator[i]){snprintf(report,sizeof report,"RAW_STACK_POOL_ALLOCATION_REJECTED segment=%u bytes=%llu rc=%d\n",i,(unsigned long long)bytes,allocation_rc);log_line(report);return 0;}
  if(mem_size(accumulator[i],&actual)||actual<bytes){log_line("RAW_STACK_POOL_SIZE_REJECTED\n");return 0;}
  if(mem_map(accumulator[i],mapped+i)||!mapped[i]){log_line("RAW_STACK_POOL_MAP_REJECTED\n");return 0;}
  segments[i]=(RsSegment){mapped[i],actual,part.height};
 }
 if(!rs_init_segments(&core,pending_config,segments,count)||
    !snapshot_budget((size_t)pending_config.height*pending_config.stride_samples*2)||!rs_async_prepare(&accumulating,&core)){
  log_line("RAW_STACK_SNAPSHOT_OR_WORKER_PREPARE_REJECTED\n");return 0;
 }
 snprintf(report,sizeof report,"RAW_STACK_SEGMENTED_ACCUMULATOR_READY count=%u bytes=%llu\n",count,(unsigned long long)core.bytes);log_line(report);
 log_line("RAW_STACK_ACCUMULATOR_ALLOCATED_BEFORE_FIRST_FRAME_ARMED\n");
 return 1;
}
static int pool_release(void){
 if(!rs_async_shutdown(&accumulating))return 0;
 int ok=1,remaining=0;
 for(unsigned i=RS_MAX_SEGMENTS;i-->0;){
  if(mapped[i]){if(mem_unmap(accumulator[i]))ok=0;else mapped[i]=NULL;}
  if(accumulator[i]&&!mapped[i]){if(mem_free(accumulator[i]))ok=0;else accumulator[i]=NULL;}
  if(accumulator[i])remaining=1;
 }
 if(!remaining&&!rs_pool_close(pool_api,&ion,&pool_module,&pool_owned))ok=0;return ok&&!ion&&!pool_module;
}
static int destination(void *context,const char *p){(void)context;return !strcmp(p,frame_path)&&access(p,F_OK)!=0;}
/* Memory route never calls this disk verifier, but private object creation requires it. */
static int no_source_file(void *context,const char *p){(void)context;(void)p;return 0;}
static void changed(void *context,X2dPrivateFrame *f){(void)context;if(f->status==X2D_FRAME_FAILED)failure("RAW_STACK_NATIVE_FRAME_REJECTED\n");}
static int claim_work(void *context,X2dPrivateFrame *f,const X2dQtShared *work){
 (void)context;if(f!=&frame||!work||!work->value||!work->control||work_ready)return 0;
 if(requested==1&&!ch_export_owned_single_header(read_api,(uintptr_t)work->value,first_header)){
  failure("RAW_STACK_REAL_HEADER_REJECTED\n");return 0;
 }
 if(iso_target){
  uint8_t current[16384];RsIsoExposure exposure;
  const uint8_t *header=first_header;
  if(requested!=1){
   if(!ch_export_owned_single_header(read_api,(uintptr_t)work->value,current)){
    failure("ISO_STACK_FRAME_HEADER_REJECTED\n");return 0;
   }
   header=current;
  }
  if(!rs_iso_exposure(header,&exposure)){
   failure("ISO_STACK_FRAME_EXPOSURE_REJECTED\n");return 0;
  }
  if(requested==1){
   iso_source=exposure;iso_planned=rs_iso_plan(exposure.iso,iso_target);
   char plan[128];snprintf(plan,sizeof plan,"RSIP1 %u %u %u %u %u\n",exposure.iso,iso_target,iso_planned,exposure.numerator,exposure.denominator);
   if(!iso_planned||!put("iso.plan",plan,1)){
    failure("ISO_STACK_TARGET_OR_PLAN_REJECTED\n");return 0;
   }
  }else if(!rs_iso_same(exposure,iso_source)||requested>iso_planned){
   failure("ISO_STACK_EXPOSURE_CHANGED_OR_EXCESS_FRAME\n");return 0;
  }
 }
 work_ready=1;return !failed;
}
static int local_notification(X2dQtShared *out){
 X2dQtString label={0};NativeCall name={base+B_STRING_UTF8,&label,{16,(uintptr_t)"x2d-raw-stack-in"}};
 x2d_qt_native_call(&name);
 void *control=FN(B_OPERATOR_NEW,void*(*)(size_t))(24),*object=control?FN(B_OPERATOR_NEW,void*(*)(size_t))(0x78):NULL;
 if(object)FN(B_LOCAL_NOTIFICATION_CONSTRUCTOR,void(*)(void*,const X2dQtString*))(object,&label);
 if(label.allocation&&__atomic_sub_fetch((int*)label.allocation,1,__ATOMIC_ACQ_REL)==0)
  FN(B_ARRAY_DEALLOCATE,void(*)(void*,intptr_t,intptr_t))(label.allocation,2,8);
 if(!object){if(control)FN(B_OPERATOR_DELETE,void(*)(void*))(control);return 0;}
 ((uint32_t*)control)[0]=((uint32_t*)control)[1]=1;
 ((uintptr_t*)control)[1]=base+B_NOTIFICATION_SHARED_DELETE;((void**)control)[2]=object;
 FN(B_SET_QOBJECT_SHARED,void(*)(void*,void*,int))(control,object,1);*out=(X2dQtShared){object,control};return 1;
}
static void answer(const X2dQtShared *notification,int success){
 X2dQtString empty={0};_Alignas(16)uint8_t value[32]={0};value[24]=2;
 if(notification&&notification->value)FN(B_NOTIFICATION_SET_RESULT,void(*)(void*,uint32_t,const X2dQtString*,const void*))(
  notification->value,success?0:1,&empty,value);
}
static void collect_frame(void);
static void dispatch_frame(void);
/* 只记录拒收依据，不放宽首帧接管。无曝光编号、照片路径或原始元数据。 */
static void log_first_claim_rejection(int snapshot,const X2dCaptureIdentity *identity,
                               const X2dExposureInfo *info){
 unsigned reasons=(!snapshot?1u:0u)|(identity->drive_mode!=0?2u:0u)|
  (identity->sequence!=0?4u:0u)|((have_last&&info->exposure_id==last_exposure)?8u:0u)|
  (failed?16u:0u);
 if(reasons&&!failed){
  char report[320];
  int n=snprintf(report,sizeof report,
   "RAW_STACK_FIRST_CLAIM_REJECT reasons=%u snapshot=%d kind=%u drive=%u sequence=%u frames=%u format=%u requested=%llu completed=%llu have_last=%u\n",
   reasons,snapshot,(unsigned)info->kind,(unsigned)identity->drive_mode,
   identity->sequence,identity->frames,(unsigned)identity->image_format,
   (unsigned long long)requested,(unsigned long long)completed,have_last);
  if(n>0&&(size_t)n<sizeof report)log_line(report);
 }
}
static int receive(void *service,const X2dExposureInfo *info,const X2dQtShared *notification){
 if(ready&&have_last&&info&&(info->exposure_id==last_exposure||(have_last>1&&info->exposure_id==previous_exposure))){answer(notification,1);return 1;}
 if(!ready||requested==completed)return FN(B_SERVICE_ADD_EXPOSURE,int(*)(void*,const void*,const void*))(service,info,notification);
 if(!info||!notification||!notification->value||!notification->control){failure("RAW_STACK_INVALID_RECEIPT\n");return 0;}
 X2dCaptureIdentity identity={0};int snapshot=x2d_capture_metadata_identity(&api,&info->metadata_map,&identity);
 if(claimed&&info->exposure_id!=frame.exposure_id){
  /* 不把额外曝光当下一帧。独占任务期间意外曝光令任务失败，原路径仍处理它。 */
  failure("RAW_STACK_UNEXPECTED_EXPOSURE\n");
  return FN(B_SERVICE_ADD_EXPOSURE,int(*)(void*,const void*,const void*))(service,info,notification);
 }
 if(!claimed){
  if(!snapshot||identity.drive_mode!=0||identity.sequence!=0||(have_last&&info->exposure_id==last_exposure)||failed){
   log_first_claim_rejection(snapshot,&identity,info);
   failure("RAW_STACK_SINGLE_FRAME_OWNERSHIP_REJECTED\n");answer(notification,0);return 1;
  }
  X2dPrivateFrameCallbacks callbacks={destination,no_source_file,changed,NULL};
  if(!x2d_private_frame_create(&frame,&api,original_cache,info->exposure_id,frame_path,callbacks)||
     !x2d_private_frame_route_memory(&frame,claim_work,NULL)){
   failure("RAW_STACK_PRIVATE_FRAME_CREATION_REJECTED\n");answer(notification,0);return 1;
  }
  claimed=1;frame_received_at=now_ms();
 }
 if(failed){answer(notification,0);return 1;}
 if(info->kind==1){
  if(held.control||!input.retain(input.context,&info->image,&held)||!input.view(input.context,&held,&view)||
     x2d_capture_raw_geometry(view.width,view.height,view.format,view.stride)!=X2D_CAPTURE_RAW_VALID||
     x2d_capture_raw_view(view.owns_allocation,view.data!=NULL,(intptr_t)view.bytes)!=X2D_CAPTURE_RAW_VALID||
     (requested==1&&!ch_metadata_copy(&owner,read_api,&info->metadata_map,&metadata))){
   failure("RAW_STACK_RAW_VIEW_OR_METADATA_REJECTED\n");answer(notification,0);return 1;
  }
 }
 X2dQtShared local={0};int ok=local_notification(&local);
 if(ok){FN(B_STORAGE_PREPARING,void(*)(void*,int))(service,1);ok=x2d_private_frame_receive(&frame,info,&local);
  FN(B_STORAGE_PREPARING,void(*)(void*,int))(service,0);FN(B_SHARED_DESTROY,void(*)(X2dQtShared*))(&local);}
 if(!ok||failed||frame.status==X2D_FRAME_FAILED){failure("RAW_STACK_MEMORY_RECEIVE_REJECTED\n");answer(notification,0);}
 else {
  answer(notification,1); /* 保留 RAW 强引用，及早完成原厂接收通知；不报原片已写盘。 */
  collect_frame();dispatch_frame(); /* 原所属线程交接/归还，不等下一轮 getter。 */
 }
 return 1;
}
static int first_metadata(void){
 if(first_ready)return 1;
 if(!ch_metadata_write_rawpb(&owner,read_api,frame.content.value,&metadata))return 0;
 X2dQtShared encoded={0};NativeCall get={base+B_FILE_PREVIEW_JPEG,&encoded,{(uintptr_t)frame.content.value}};
 x2d_qt_native_call(&get);if(!encoded.value||!encoded.control)return 0;
 RawView data={0};NativeCall bytes={base+B_IMAGE_RAW_DATA,&data,{(uintptr_t)encoded.value}};x2d_qt_native_call(&bytes);
 size_t length=data.length>0?capture_preview_stream_size(data.data,(size_t)data.length):0;
 if(length){preview=malloc(length);if(preview){memcpy(preview,data.data,length);preview_bytes=length;}}
 if(data.allocation&&__atomic_sub_fetch((int*)data.allocation,1,__ATOMIC_ACQ_REL)==0)
  FN(B_ARRAY_DEALLOCATE,void(*)(void*,intptr_t,intptr_t))(data.allocation,1,8);
 FN(B_SHARED_DESTROY,void(*)(X2dQtShared*))(&encoded);
 first_ready=preview!=NULL;return first_ready;
}
static int metadata_file(void){
 char p[256];if(!metadata.bytes||!path(p,sizeof p,"rawpb.bin"))return 0;
 int fd=open(p,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return 0;
 size_t done=0;while(done<metadata.size){ssize_t n=write(fd,metadata.bytes+done,metadata.size-done);if(n<=0){close(fd);return 0;}done+=(size_t)n;}
 int ok=fsync(fd)==0;if(close(fd))ok=0;return ok;
}
static int frame_slot_available(void){
 return !claimed&&(unsigned)(waiting_held.control!=NULL)+(unsigned)(accumulating_held.control!=NULL)<2;
}
/* 普通内存快照复制完成就归还原厂 RAW；累计工人继续使用自己的快照。 */
static void collect_frame(void){
 if(!input.allowed||!input.allowed(input.context))return;
 if(accumulating_held.control&&rs_async_source_read(&accumulating)){
  input.release(input.context,&accumulating_held);
  RsAsyncTiming timing={0};rs_async_timing(&accumulating,&timing);
  char report[192];snprintf(report,sizeof report,"RAW_STACK_SOURCE_RELEASED sequence=%llu read_ms=%llu copy_ms=%llu\n",
   (unsigned long long)accumulating_sequence,(unsigned long long)(now_ms()-accumulating_started),(unsigned long long)timing.copy_ms);log_line(report);
 }
 int ok=0;
 if(rs_async_collect(&accumulating,&ok)==1){
  if(accumulating_held.control)input.release(input.context,&accumulating_held);
  if(!ok){failure("RAW_STACK_ACCUMULATION_REJECTED\n");return;}
  RsAsyncTiming timing={0};rs_async_timing(&accumulating,&timing);
  char command[64],report[192];snprintf(report,sizeof report,"RAW_STACK_FRAME_ACCUMULATED frames=%llu total_ms=%llu worker_ms=%llu\n",
   (unsigned long long)core.frames,(unsigned long long)(now_ms()-accumulating_started),(unsigned long long)timing.total_ms);log_line(report);
  snprintf(command,sizeof command,"RS1 %llu\n",(unsigned long long)accumulating_sequence);
  if(!put("frame.done",command,0))failure("RAW_STACK_FRAME_DONE_REJECTED\n");
 }
}
static void start_waiting_frame(void){
 if(failed||accumulating.active||!waiting_held.control)return;
 accumulating_held=waiting_held;memset(&waiting_held,0,sizeof waiting_held);
 accumulating_sequence=waiting_sequence;accumulating_started=now_ms();
 if(!rs_async_start(&accumulating,&core,(const uint16_t*)waiting_view.data,waiting_view.bytes)){
  input.release(input.context,&accumulating_held);failure("RAW_STACK_ACCUMULATION_THREAD_REJECTED\n");return;
 }
 memset(&waiting_view,0,sizeof waiting_view);
 char report[160];snprintf(report,sizeof report,"RAW_STACK_SNAPSHOT_STARTED sequence=%llu\n",
  (unsigned long long)accumulating_sequence);log_line(report);
}
/* 一个后台快照 + 最多两份原厂 RAW 引用；接收、复制和累计不互相串行等待。 */
static void dispatch_frame(void){
 if(failed||output_done||!input.allowed||!input.allowed(input.context))return;
 start_waiting_frame();if(failed)return;
 if(claimed&&held.control&&work_ready&&!waiting_held.control){
  if(requested==1&&!first_metadata()){failure("RAW_STACK_FINAL_METADATA_REJECTED\n");return;}
  uint32_t exposure=frame.exposure_id;
  if(!x2d_private_frame_finish_memory(&frame,1)||!x2d_private_frame_release(&frame)){
   failure("RAW_STACK_PRIVATE_FRAME_DRAIN_REJECTED\n");return;
  }
  waiting_held=held;memset(&held,0,sizeof held);waiting_view=view;memset(&view,0,sizeof view);
  waiting_sequence=requested;
  previous_exposure=last_exposure;last_exposure=exposure;if(have_last<2)have_last++;
  memset(&frame,0,sizeof frame);claimed=work_ready=0;completed=requested;
  start_waiting_frame();if(failed)return;
  char report[192],command[64];snprintf(report,sizeof report,"RAW_STACK_FRAME_QUEUED sequence=%llu receive_to_queue_ms=%llu\n",
   (unsigned long long)completed,(unsigned long long)(now_ms()-frame_received_at));log_line(report);
  snprintf(command,sizeof command,"RS1 %llu\n",(unsigned long long)completed);
  if(!put("frame.accepted",command,0))failure("RAW_STACK_FRAME_ACCEPTED_REJECTED\n");
 }
}
#ifdef X2D_QUARTER_MEMORY
#include "quarter_memory_capture.inc"
#endif
static void pump(void){
 if(failed||output_done)return;
#ifdef X2D_QUARTER_MEMORY
 if(quarter_pending){quarter_drain();return;}
#endif
 char command[64];uint64_t sequence=0;const char *tail=command+4;
 collect_frame();if(failed)return;
 if(requested==completed&&text_read("frame.request",command,sizeof command)>0&&
    !strncmp(command,"RS1 ",4)&&parse_uint(&tail,&sequence,'\n')&&!*tail){
  char canonical[64];int n=snprintf(canonical,sizeof canonical,"RS1 %llu\n",(unsigned long long)sequence);
  if(n<=0||strcmp(canonical,command)||sequence!=completed+1){
   if(sequence!=completed)failure("RAW_STACK_FRAME_SEQUENCE_REJECTED\n");
  }else if(sequence>RS_MAX_FRAMES){
   failure("RAW_STACK_FRAME_LIMIT_REJECTED\n");
  }else if(frame_slot_available()){
   if(!pool_allocate()){failure("RAW_STACK_ACCUMULATOR_AT_SHUTTER_REJECTED\n");return;}
   requested=sequence;if(!put("frame.armed",command,0))failure("RAW_STACK_ARM_MARKER_REJECTED\n");
  }
 }
 dispatch_frame();
 if(!failed&&requested==completed&&!claimed&&!waiting_held.control&&!accumulating.active&&text_read("finish.request",command,sizeof command)==7&&!strcmp(command,"FINISH\n")){
  size_t bytes=0;RsPixelBlock blocks[RS_MAX_SEGMENTS];
  unsigned count=rs_parallel_finish_blocks(&core,4,blocks,RS_MAX_SEGMENTS,&bytes);char p[256];
#ifdef X2D_QUARTER_MEMORY
  (void)p;
  if(!count||bytes!=210510336||!first_ready||!quarter_output(blocks,count))failure("QUARTER_MEMORY_FINAL_OUTPUT_REJECTED\n");
  return;
#endif
  if(!count||bytes!=210510336||!first_ready||!path(p,sizeof p,"merged.3FR")||
     !rs_raw_write_blocks(p,first_header,blocks,count,preview,preview_bytes)||!metadata_file()||!pool_release()){
   failure("RAW_STACK_FINAL_RAW_OR_RELEASE_REJECTED\n");return;
  }
  if(iso_target){
   RsIsoResult result;char record[160];
   if(core.frames>UINT32_MAX||!rs_iso_result(iso_source,iso_target,(uint32_t)core.frames,&result)){
    failure("ISO_STACK_RESULT_EXPOSURE_REJECTED\n");return;
   }
   snprintf(record,sizeof record,"RSI1 %u %u %u %u %u %u\n",iso_source.iso,iso_target,iso_source.numerator,iso_source.denominator,result.frames,result.planned);
   if(!put("iso.result",record,1)){failure("ISO_STACK_RESULT_RECORD_REJECTED\n");return;}
  }
  output_done=1;log_line("RAW_STACK_FINAL_RAW_WRITTEN_ALL_SOURCE_REFERENCES_RELEASED\n");
  if(!put("raw.ready","RAW_STACK_R1\n",1))failure("RAW_STACK_RAW_READY_REJECTED\n");
 }
}
static uint32_t getter(const void *service){
 uint32_t result=FN(B_STORAGE_CAPTURE_MODE,uint32_t(*)(const void*))(service);
 if(!ready&&!failed){
  original_cache=*(void**)((const uint8_t*)service+0xe0);
  if(!original_cache||*(uintptr_t*)original_cache!=base+B_CACHE_VTABLE+16){failure("RAW_STACK_ORIGINAL_CACHE_REJECTED\n");return result;}
  RsConfig config={11836,8842,11904,RS_AVERAGE,0};char value[32];uint64_t method,noise;const char *tail=value+4;
  if(text_read("stack.config",value,sizeof value)<=0||strncmp(value,"RS1 ",4)||
     !parse_uint(&tail,&method,' ')||!parse_uint(&tail,&noise,'\n')||*tail||
     method>2||noise>1||(method==2&&noise)||!cm_native_bind(&owner,base,(void*)service,&input)||
     !ch_native_open(&reader,&owner,&read_api)){
   failure("RAW_STACK_CONFIG_OR_NATIVE_BIND_REJECTED\n");return result;
  }
  config.method=(RsMethod)method;config.noise_group=noise?4:0;
  char target_text[32];int target_bytes=text_read("iso.target",target_text,sizeof target_text);
  if(target_bytes>0){
   const char *target_tail=target_text;uint64_t target;
   if(method!=RS_AVERAGE||noise||!parse_uint(&target_tail,&target,'\n')||*target_tail||!target||target>65535){
    failure("ISO_STACK_TARGET_REJECTED\n");return result;
   }
   iso_target=(uint32_t)target;
  }else{char target_path[256];if(!path(target_path,sizeof target_path,"iso.target")||access(target_path,F_OK)==0){
   failure("ISO_STACK_TARGET_UNREADABLE\n");return result;
  }}
  char canonical[32];snprintf(canonical,sizeof canonical,"RS1 %u %u\n",(unsigned)method,(unsigned)noise);
  if(strcmp(value,canonical)){failure("RAW_STACK_CONFIG_NOT_CANONICAL\n");return result;}
  if(!pool_prepare(config)||!path(frame_path,sizeof frame_path,"frame-0.3FR")||access(frame_path,F_OK)==0||
     !put("hook.ready","RAW_STACK_PREPARED\n",1)){failure("RAW_STACK_ACCUMULATOR_PREPARE_REJECTED\n");return result;}
  ready=1;log_line("RAW_STACK_PREPARED_NO_EXPOSURE_SUBMITTED\n");
 }
 if(ready)pump();return result;
}
static int image_base(struct dl_phdr_info *image,size_t size,void *out){
 (void)size;if(image->dlpi_name&&(!image->dlpi_name[0]||!strcmp(image->dlpi_name,"/system/bin/camera-storage"))){*(uintptr_t*)out=image->dlpi_addr;return 1;}return 0;
}
static int replace(uintptr_t address,uintptr_t value){
 int fd=open("/proc/self/mem",O_RDWR);if(fd<0)return 0;int ok=pwrite(fd,&value,sizeof value,(off_t)address)==sizeof value;close(fd);
 return ok&&*(uintptr_t*)address==value;
}
__attribute__((constructor))static void install(void){
 const char *enabled=getenv("X2D_RAW_STACK_ABI");if(!enabled||strcmp(enabled,"1")||!capture_runtime_job(job,getenv("X2D_RAW_STACK_JOB")))return;
 char exe[128];ssize_t n=readlink("/proc/self/exe",exe,sizeof exe-1);if(n<=0||n>=(ssize_t)sizeof exe-1)return;exe[n]=0;
 if(strcmp(exe,"/system/bin/camera-storage"))return;
 int directory=open(job,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(directory<0)return;close(directory);
 char p[256];if(!path(p,sizeof p,"capture.log"))return;log_fd=open(p,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(log_fd<0)return;
 dl_iterate_phdr(image_base,&base);if(!base||!x2d_private_frame_bind(&api,base)){failure("RAW_STACK_ABI_REJECTED\n");return;}
 uintptr_t table=base+B_SERVICE_VTABLE+16;
 if(*(uintptr_t*)(table+0x80)!=base+B_STORAGE_CAPTURE_MODE||*(uintptr_t*)(table+0x178)!=base+B_SERVICE_ADD_EXPOSURE||
    !replace(table+0x178,(uintptr_t)receive)){
  failure("RAW_STACK_STORAGE_HOOK_REJECTED\n");return;
 }
 if(!replace(table+0x80,(uintptr_t)getter)){(void)replace(table+0x178,base+B_SERVICE_ADD_EXPOSURE);failure("RAW_STACK_PUMP_HOOK_REJECTED\n");}
}
