#include "disk_abort_io.h"
static int disk_abort_poll(void *service){
 int request=da_read(X2D_PRIVATE_CAPTURE_JOB,"abort.request","SIXTEEN_ABORT_IDLE_V1\n");
 if(!request)return 0;
 aborting=1;
 if(request<0){failure("SIXTEEN_ABORT_REQUEST_REJECTED\n");return 1;}
 if(abort_done)return 1;
 if(api.qobject_thread(service)!=api.current_thread()){
  log_line("SIXTEEN_ABORT_OWNER_THREAD_UNCONFIRMED\n");return 1;
 }
 /* release 自身核对 owner 线程、原厂 flushed / 未接收输入；不伪造 flush。 */
 for(unsigned i=0;i<X2D_PRIVATE_CAPTURE_FRAMES;i++){
  CaptureFrame *f=frames+i;if(f->released)continue;
  if(!x2d_private_frame_release(&f->frame)){
   log_line("SIXTEEN_ABORT_PRIVATE_FRAME_STILL_OWNED\n");return 1;
  }
  if(f->original_raw.control)FN(B_SHARED_DESTROY,void(*)(X2dQtShared*))(&f->original_raw);
  memset(&f->original_raw,0,sizeof f->original_raw);memset(&f->pixels,0,sizeof f->pixels);f->released=1;
 }
 ch_metadata_destroy(&disk_metadata);
 if(!da_receipt(X2D_PRIVATE_CAPTURE_JOB,"SIXTEEN_ABORT_DRAINED_V1\n")){
  log_line("SIXTEEN_ABORT_RECEIPT_FAILED\n");return 1;
 }
 abort_done=1;log_line("SIXTEEN_ABORT_ALL_SOURCE_REFERENCES_RELEASED\n");return 1;
}
