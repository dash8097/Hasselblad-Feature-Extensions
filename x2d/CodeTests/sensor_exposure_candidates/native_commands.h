static int consume_commands(int listener){
 /* 原厂允许的本地数据报；每包验证内核凭据，单次最多四包。 */
 for(unsigned i=0;i<4;i++){
  X2dFlashRequest request;struct sockaddr_un sender={0};
  union {struct cmsghdr alignment;unsigned char data[CMSG_SPACE(sizeof(struct ucred))];} control={0};
  struct iovec vector={&request,sizeof request};struct msghdr message={0};
  message.msg_name=&sender;message.msg_namelen=sizeof sender;message.msg_iov=&vector;message.msg_iovlen=1;message.msg_control=control.data;message.msg_controllen=sizeof control.data;
  long n=recvmsg(listener,&message,MSG_DONTWAIT|MSG_TRUNC);
  if(n<0&&(*__errno()==11||*__errno()==4))break;
  if(n<0)return fail("native_receive_failed");
  struct cmsghdr*header=CMSG_FIRSTHDR(&message);struct ucred credentials={0};
  if(n!=(long)sizeof request||(message.msg_flags&(MSG_TRUNC|MSG_CTRUNC))||!header||header->cmsg_level!=SOL_SOCKET||header->cmsg_type!=SCM_CREDENTIALS||header->cmsg_len!=CMSG_LEN(sizeof credentials)||CMSG_NXTHDR(&message,header))continue;
  memcpy(&credentials,CMSG_DATA(header),sizeof credentials);
  if(message.msg_namelen<=offsetof(struct sockaddr_un,sun_path)+1||message.msg_namelen>sizeof sender||sender.sun_family!=AF_UNIX||sender.sun_path[0]||!native_peer(credentials)||!x2d_flash_request_valid(&request,core.sequence))continue;
  if(request.operation){
   char command[128];snprintf(command,sizeof command,"X2D_SENSOR_FLASH 1 %u CONFIG 3 %u %u",request.sequence,request.delay_us,request.profile?1u:0u);
   native_profile=request.profile;if(!command_line(command))return 0;
  }
  page_status(&core,"native",allowed_mask,sampling,helper_ok,actual_ns);
  /* 回复丢失不撤销已执行设置；客户端只查询，不重放。 */
  (void)sendto(listener,&native_reply,sizeof native_reply,MSG_NOSIGNAL|MSG_DONTWAIT,(struct sockaddr*)&sender,message.msg_namelen);
 }
 return 1;
}
static unsigned command_used; /* 兼容共享 worker 的完整命令门控；本协议每包完整。 */
