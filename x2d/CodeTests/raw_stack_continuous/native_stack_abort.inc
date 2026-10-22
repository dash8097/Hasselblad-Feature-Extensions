/* 固定控制协议；只由 storage owner getter 线程处理。
 * runner 写请求前必须等自己的 RPC 结束并确认曝光/存储 idle；本层仍逐项检查本地所有权。
 * 不为 pending/未提交的原厂对象伪造完成，无法释放时保留资源且没有成功回执。
 */
static int stack_abort_poll(void){
 char token[64];int n=text_read("abort.request",token,sizeof token);
 if(n<0){char p[256];if(!path(p,sizeof p,"abort.request")||access(p,F_OK)==0){failure("RAW_STACK_ABORT_REQUEST_UNREADABLE\n");return 1;}return 0;}
 if(strcmp(token,"RAW_STACK_ABORT_IDLE_V1\n")){failure("RAW_STACK_ABORT_REQUEST_REJECTED\n");return 1;}
 if(abort_done)return 1;
 aborting=1;
 if(dma_snapshot.uncertain){log_line("RAW_STACK_ABORT_DMA_COMPLETION_UNCONFIRMED\n");return 1;}
 if(!input.allowed||!input.allowed(input.context)){log_line("RAW_STACK_ABORT_OWNER_THREAD_UNCONFIRMED\n");return 1;}
 /* 不再接受新的 frame.request；当前累计只等待自然完成，不杀工人或归还它正在读的 RAW。 */
 if(accumulating.active){int ok=0;int done=rs_async_collect(&accumulating,&ok);if(done!=1)return 1;(void)ok;}
 if(accumulating_held.control)input.release(input.context,&accumulating_held);
 if(accumulating_held.control){log_line("RAW_STACK_ABORT_ACCUMULATING_REFERENCE_UNCONFIRMED\n");return 1;}
 if(waiting_held.control)input.release(input.context,&waiting_held);
 if(waiting_held.control){log_line("RAW_STACK_ABORT_WAITING_REFERENCE_UNCONFIRMED\n");return 1;}
 memset(&waiting_view,0,sizeof waiting_view);
 /* 已经收到原厂输入却未提交 private writer 的 frame 不能强行销毁。 */
 if(frame.status!=X2D_FRAME_EMPTY&&!frame.memory_finished&&frame.submitted){
  if(!x2d_private_frame_finish_memory(&frame,0)){log_line("RAW_STACK_ABORT_PRIVATE_FINISH_UNCONFIRMED\n");return 1;}
 }
 if(!x2d_private_frame_release(&frame)){log_line("RAW_STACK_ABORT_PRIVATE_RELEASE_UNCONFIRMED\n");return 1;}
 if(held.control)input.release(input.context,&held);
 if(held.control){log_line("RAW_STACK_ABORT_CURRENT_REFERENCE_UNCONFIRMED\n");return 1;}
 memset(&view,0,sizeof view);claimed=work_ready=0;
 if(!pool_release()){log_line("RAW_STACK_ABORT_POOL_RELEASE_UNCONFIRMED\n");return 1;}
 ch_metadata_destroy(&metadata);free(preview);preview=NULL;preview_bytes=0;
 requested=completed;output_done=1;
 if(!put("abort.drained","RAW_STACK_ABORT_DRAINED_V1\n",1)){log_line("RAW_STACK_ABORT_RECEIPT_WRITE_FAILED\n");return 1;}
 abort_done=1;log_line("RAW_STACK_ABORT_ALL_SOURCE_REFERENCES_AND_POOL_RELEASED\n");return 1;
}
