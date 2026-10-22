/* 真实通知 -> CsSession -> 最终 RAW/显影 socket。六张原片仅保留内存。
 * 只由 storage owner getter pump；本文件不发曝光，不修改菜单或发布相册。
 * 接收回复与 RAW 生命周期分开：接入内存后回复，真实强引用持续到消费结束。
 * 成片完成仍使用独立 memory.ready，不伪造原片写盘完成信号。 */
#include "capture_memory_native.h"
#include "capture_header_native.h"
#include "capture_header_tap.h"
#include "capture_render_stream.h"
#include "capture_merged_header.h"
#include "capture_metadata_native.h"
#include "capture_band_wire.h"
#if X2D_PRIVATE_CAPTURE_FRAMES != 6
#error Memory session requires a real six-shot request
#endif
static CmNative memory_native;
static ChNative memory_read;
static ChReadApi memory_read_api;
static ChTap memory_header;
static ChMetadata memory_metadata;
static CsSession *memory_session;
static CsRawOutput memory_raw;
static CsRenderStream memory_render;
static CsOutputApi memory_actual;
static unsigned memory_failed,memory_issued,memory_terminal,memory_inputs,memory_inputs_ready,memory_preview_ready;
static unsigned memory_sources_done,memory_sources_failed;
static uint64_t memory_started;
static uint64_t memory_merge_started;
static uint64_t memory_now(void){
 struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)||t.tv_sec<0||t.tv_nsec<0||t.tv_nsec>=1000000000)return 0;
 return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
static int memory_begin(void *context,const uint8_t *preview,size_t bytes,unsigned width,unsigned height,
 CwConsumer consumers[PC_CONSUMERS]){
 (void)context;uint8_t merged[4096];char raw_path[256],socket_path[108];CsRawRenderApi render;
 if(!memory_header.ready||memory_header.failed||!memory_metadata.bytes){log_line("MEMORY_OUTPUT_INPUT_NOT_READY\n");return 0;}
 const char *header_error=NULL;
 if(!capture_merged_header_checked(memory_header.header,merged,&header_error)){
  log_line(header_error?header_error:"MEMORY_OUTPUT_MERGED_HEADER_REJECTED\n");return 0;}
 int a=snprintf(raw_path,sizeof raw_path,"%s/merged.3FR",X2D_PRIVATE_CAPTURE_JOB);
 int b=snprintf(socket_path,sizeof socket_path,"%s",CBW_SOCKET_PATH);
 if(a<=0||a>=(int)sizeof raw_path||b<=0||b>=(int)sizeof socket_path){log_line("MEMORY_OUTPUT_PATH_REJECTED\n");return 0;}
 if(!cs_render_stream_bind_full(&memory_render,1,socket_path,memory_metadata.bytes,memory_metadata.size,&render)){
  log_line("MEMORY_OUTPUT_METADATA_BIND_REJECTED\n");return 0;}
 if(!cs_raw_output_bind(&memory_raw,1,raw_path,memory_header.header,merged,render,&memory_actual)){
  log_line("MEMORY_OUTPUT_RAW_BIND_REJECTED\n");return 0;}
 int ok=memory_actual.begin(memory_actual.context,preview,bytes,width,height,consumers);
 if(ok)memory_merge_started=memory_now();
 if(!ok)log_line(memory_raw.begin_error?memory_raw.begin_error:memory_raw.file?"MEMORY_OUTPUT_NATIVE_CONNECT_REJECTED\n":"MEMORY_OUTPUT_RAW_CREATE_REJECTED\n");
 else log_line("MEMORY_OUTPUT_RAW_AND_NATIVE_CHANNEL_STARTED\n");
 return ok;
}
static int memory_finish(void *context){
 (void)context;
 if(!memory_sources_done||memory_sources_failed)return 0;
 for(unsigned i=0;i<6;i++)if(!frames[i].released)return 0;
 log_line("MEMORY_NATIVE_SOURCES_RELEASED_BEFORE_RENDER_FINISH\n");
 return memory_actual.finish&&memory_actual.finish(memory_actual.context);
}
static void memory_abort(void *context){(void)context;if(memory_actual.abort)memory_actual.abort(memory_actual.context);}
static uint16_t *memory_full_pixels(void *context,size_t bytes){
 (void)context;return cs_render_stream_full_pixels(&memory_render,bytes);
}
static int memory_source_gate(void *context){
 (void)context;if(!memory_sources_done||memory_sources_failed)return 0;
 for(unsigned i=0;i<6;i++)if(!frames[i].released)return 0;
 return 1;
}
static int memory_prepare(void *service){
 CmApi input;
 if(!cm_native_bind(&memory_native,base,service,&input)||
    !ch_native_open(&memory_read,&memory_native,&memory_read_api))return 0;
 CsConfig config={.task=1,.timeout_ms=300000,.width=11663,.height=8749,.batch=1024,
  .working_limit=256u*1024u*1024u,.input=input,
  .output={memory_begin,memory_finish,memory_abort,NULL},.compute_threads=4,.compact_output=1,
  .cache_rows=1024,.full_output=1,.full_pixels=memory_full_pixels,.sources_released=memory_source_gate};
 memory_session=cs_create(&config);
 if(!memory_session){(void)ch_native_close(&memory_read);return 0;}return 1;
}
static void memory_failure(void){
 if(!memory_failed){memory_failed=1;if(memory_session)(void)cs_cancel(memory_session);
  log_line("MEMORY_SESSION_CANCELLED_NO_SOURCE_RAW_FILEIO\n");(void)owned_marker("memory.failed");}
}
static void memory_issue(void){
 memory_started=memory_now();
 if(!memory_started||!cs_request_issued(memory_session,1,memory_started)){memory_failure();return;}
 memory_issued=1;
}
static void memory_receive(CaptureFrame *frame,const X2dExposureInfo *info){
 if(!memory_issued||memory_failed)return;
 /* kind=0 可以是待编码 YUV。保留原厂预览编码，后续只从 previewJpeg
  * 取得格式 8 的真实编码结果，不把原始预览误标为已完成 JPEG。 */
 if(info->kind==0)return;
 X2dCaptureIdentity identity={0};
 int snapshot=x2d_capture_metadata_identity(&api,&info->metadata_map,&identity);
 if(frame->index==0&&!memory_header.attached){
  if(!snapshot||!ch_tap_attach(&memory_header,&frame->frame,&identity,memory_read_api)){
   memory_failure();return;
  }
 }
 if(frame->index==0&&info->kind==1&&!memory_metadata.bytes&&
    !ch_metadata_copy(&memory_native,memory_read_api,&info->metadata_map,&memory_metadata)){
  memory_failure();return;
 }
 if(cs_receive(memory_session,info,snapshot?&identity:NULL,info->kind==0)==X2D_CAPTURE_CONFLICT)memory_failure();
}
static int memory_take_encoded_preview(void){
 CaptureFrame *frame=frames;X2dQtShared encoded={0};
 if(!frame->frame.content.value)return 0;
 NativeCall get={.function=base+B_FILE_PREVIEW_JPEG,.result=&encoded,
  .args={(uintptr_t)frame->frame.content.value}};
 x2d_qt_native_call(&get);
 if(!encoded.value||!encoded.control)return 0;
 X2dExposureInfo preview={.exposure_id=frame->frame.exposure_id,.kind=0,.image=encoded};
 int classification=cs_receive(memory_session,&preview,NULL,1);
 FN(B_SHARED_DESTROY,void(*)(X2dQtShared*))(&encoded);
 if(classification!=X2D_CAPTURE_OWN){memory_failure();return 0;}
 memory_preview_ready=1;return 1;
}
static int memory_claim_work(void *context,X2dPrivateFrame *native,const X2dQtShared *work){
 CaptureFrame *frame=context;
 if(!frame||native!=&frame->frame||frame->index>=6||!work||!work->control)return 0;
 memory_inputs|=1u<<frame->index;
 return !memory_failed;
}
static int memory_complete_metadata(void){
 CaptureFrame *frame=frames;
 if(!frame->frame.content.value||!memory_header.ready||!memory_metadata.bytes){
  log_line("MEMORY_RAWPB_SOURCE_PREREQUISITE_REJECTED\n");return 0;
 }
 int ok=ch_metadata_write_rawpb(&memory_native,memory_read_api,frame->frame.content.value,&memory_metadata);
 if(!ok){log_line("MEMORY_RAWPB_REAL_WHITE_BALANCE_REJECTED\n");return 0;}
 log_line("MEMORY_RAWPB_REAL_WHITE_BALANCE_VALIDATED\n");return 1;
}
static int memory_local_notification(X2dQtShared *out){
 X2dQtString label={0};NativeCall name={.function=base+B_STRING_UTF8,.result=&label,
  .args={strlen("x2d-memory-source"),(uintptr_t)"x2d-memory-source"}};
 x2d_qt_native_call(&name);
 void *control=FN(B_OPERATOR_NEW,void*(*)(size_t))(24);
 void *object=control?FN(B_OPERATOR_NEW,void*(*)(size_t))(0x78):NULL;
 if(!object){if(control)FN(B_OPERATOR_DELETE,void(*)(void*))(control);
  if(label.allocation&&__atomic_sub_fetch((int*)label.allocation,1,__ATOMIC_ACQ_REL)==0)
   FN(B_ARRAY_DEALLOCATE,void(*)(void*,intptr_t,intptr_t))(label.allocation,2,8);
  return 0;
 }
 FN(B_LOCAL_NOTIFICATION_CONSTRUCTOR,void(*)(void*,const X2dQtString*))(object,&label);
 if(label.allocation&&__atomic_sub_fetch((int*)label.allocation,1,__ATOMIC_ACQ_REL)==0)
  FN(B_ARRAY_DEALLOCATE,void(*)(void*,intptr_t,intptr_t))(label.allocation,2,8);
 uint32_t *counts=control;counts[0]=counts[1]=1;
 ((uintptr_t*)control)[1]=base+B_NOTIFICATION_SHARED_DELETE;((void**)control)[2]=object;
 FN(B_SET_QOBJECT_SHARED,void(*)(void*,void*,int))(control,object,1);
 *out=(X2dQtShared){object,control};return 1;
}
static int memory_accept_notification(CaptureFrame *frame,const X2dExposureInfo *info,
 const X2dQtShared *notification){
 if(memory_failed||frame->memory_accepted_receipts>=16||
    (info->kind==1&&!cs_source_retained(memory_session,frame->index,info->exposure_id)))return 0;
 X2dQtShared local={0};if(!memory_local_notification(&local))return 0;
 int accepted=x2d_private_frame_receive(&frame->frame,info,&local);
 FN(B_SHARED_DESTROY,void(*)(X2dQtShared*))(&local);
 if(!accepted||frame->frame.status==X2D_FRAME_FAILED||memory_failed)return 0;
 /* Cs 已保留验证过的 RAW；原厂私有 Content 保留其他接收数据。
  * 接收请求必须及时完成，不能等待它自身可能阻塞的元数据/预览。
  * 不 retain 真实通知；原调用者释放最后一份时回复正常存储接收请求。
  * 这不是 fileFlushed，内存引用仍由 Cs/Content 独立持有。 */
 X2dQtString empty={0};_Alignas(16) uint8_t result[32]={0};result[24]=2;
 FN(B_NOTIFICATION_SET_RESULT,void(*)(void*,uint32_t,const X2dQtString*,const void*))(
  notification->value,0,&empty,result);
 frame->memory_accepted_receipts++;
 char diagnostic[192];int n=snprintf(diagnostic,sizeof diagnostic,
  "MEMORY_FRAME_ACCEPTED index=%u kind=%u raw_work_mask=%u first_header=%u metadata=%u\n",
  frame->index,(unsigned)info->kind,memory_inputs,memory_header.ready,(unsigned)(memory_metadata.bytes!=NULL));
 if(n>0&&(size_t)n<sizeof diagnostic)log_line(diagnostic);
 return accepted;
}
static void memory_drain_frames(void){
 if(!memory_sources_done)return;
 for(unsigned i=0;i<6;i++){
  CaptureFrame *frame=frames+i;if(frame->released)continue;
  if(frame->frame.status!=X2D_FRAME_EMPTY){
   if(!x2d_private_frame_finish_memory(&frame->frame,!memory_sources_failed)||
      !x2d_private_frame_release(&frame->frame))continue;
  }
  frame->released=1;
  log_line("MEMORY_SOURCE_REFERENCES_RELEASED_AFTER_CONSUMERS\n");
 }
}
static void memory_pump(void){
 if(!memory_session||memory_terminal||!memory_issued)return;
 uint64_t now=memory_now();
 if(!now||now<memory_started||now-memory_started>=300000||memory_header.failed||failed)memory_failure();
 /* 头部和独占显影端点均准备好后才启动输出；等候期间仍有明确总期限。 */
 if(!memory_failed){
  if(!memory_header.ready||memory_inputs!=63)return;
  if(!memory_preview_ready&&!memory_take_encoded_preview())return;
  if(!memory_inputs_ready){
   if(!memory_complete_metadata()||!owned_marker("memory.inputs.ready")){memory_failure();return;}
   memory_inputs_ready=1;log_line("MEMORY_SIX_NATIVE_HEADERS_AND_RAWPB_METADATA_READY_NO_SOURCE_FILES\n");
  }
  char endpoint[108];int length=snprintf(endpoint,sizeof endpoint,"%s",CBW_SOCKET_PATH);
  if(length<=0||length>=(int)sizeof endpoint){memory_failure();return;}
  if(!memory_raw.started&&access(endpoint,F_OK)!=0)return;
 }
 int result=cs_step(memory_session,now);
 if(cs_sources_consumed(memory_session)){
  if(!memory_sources_done){
   char timing[256];uint64_t end=memory_now();
   snprintf(timing,sizeof timing,"MERGE_COMPUTE_MS %llu\nMERGE_INPUT_COPY_MS %llu\nMERGE_CACHED_COMPUTE_MS %llu\nMERGE_TOTAL_MS %llu\n",
    (unsigned long long)cs_compute_ms(memory_session),
    (unsigned long long)cs_input_copy_ms(memory_session),
    (unsigned long long)cs_cached_compute_ms(memory_session),
    (unsigned long long)(memory_merge_started&&end>=memory_merge_started?end-memory_merge_started:0));
   log_line(timing);
  }
  memory_sources_done=1;memory_drain_frames();
 }
 if(result==CS_COMPLETE){
  if(!owned_marker("memory.ready")){
   memory_failed=1;(void)owned_marker("memory.failed");log_line("MEMORY_SESSION_READY_MARKER_FAILED\n");
  }else log_line("MEMORY_SESSION_RAW_AND_NATIVE_JPEG_COMPLETED\n");
  memory_terminal=1;
 }else if(result==CS_FAILED){log_line(cs_failure_stage(memory_session));memory_failed=memory_terminal=1;(void)owned_marker("memory.failed");}
 if(memory_terminal){
  if(!cs_destroy(memory_session)){log_line("MEMORY_SESSION_DRAIN_RELEASE_REJECTED\n");return;}
  memory_session=NULL;
  if(!memory_sources_done){memory_sources_done=1;memory_sources_failed=memory_failed;}
  ch_metadata_destroy(&memory_metadata);
  if(!ch_native_close(&memory_read))log_line("MEMORY_HEADER_READER_CLOSE_REJECTED\n");
  memory_drain_frames();
 }
}
