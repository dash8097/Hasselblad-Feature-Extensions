#include "disk_abort_io.h"
static int quarter_abort_poll(void *service){
 int request=da_read(job,"abort.request","QUARTER_ABORT_IDLE_CONSUMER_DRAINED_V1\n");
 if(!request)return 0;
 aborting=1;
 if(request<0){failure("QUARTER_ABORT_REQUEST_REJECTED\n");return 1;}
 if(abort_done)return 1;
 if(api.qobject_thread(service)!=api.current_thread()||(input.allowed&&!input.allowed(input.context))){
  log_line("QUARTER_ABORT_OWNER_THREAD_UNCONFIRMED\n");return 1;
 }
 /* runner 排空显影进程后仍等本库计算线程自然结束，不 TERM 工人。 */
 if(accumulating.active){int ok=0;if(rs_async_collect(&accumulating,&ok)!=1)return 1;}
 if(accumulating_held.control)input.release(input.context,&accumulating_held);
 if(accumulating_held.control)return 1;
 if(waiting_held.control)input.release(input.context,&waiting_held);
 if(waiting_held.control)return 1;
 memset(&waiting_view,0,sizeof waiting_view);
 if(frame.status!=X2D_FRAME_EMPTY&&!frame.memory_finished&&frame.submitted){
  if(!x2d_private_frame_finish_memory(&frame,0))return 1;
 }
 /* 未提交的半帧不冒充提交；release 拒绝仍有原厂引用的状态。 */
 if(!x2d_private_frame_release(&frame)){
  log_line("QUARTER_ABORT_PRIVATE_FRAME_STILL_OWNED\n");return 1;
 }
 if(held.control)input.release(input.context,&held);if(held.control)return 1;
 memset(&view,0,sizeof view);claimed=work_ready=0;
 if(!pool_release()){log_line("QUARTER_ABORT_POOL_RELEASE_UNCONFIRMED\n");return 1;}
 ch_metadata_destroy(&metadata);free(preview);preview=NULL;preview_bytes=0;
 requested=completed;output_done=1;quarter_pending=0;
 if(!da_receipt(job,"QUARTER_ABORT_DRAINED_V1\n")){
  log_line("QUARTER_ABORT_RECEIPT_FAILED\n");return 1;
 }
 abort_done=1;log_line("QUARTER_ABORT_ALL_SOURCE_REFERENCES_AND_POOL_RELEASED\n");return 1;
}
