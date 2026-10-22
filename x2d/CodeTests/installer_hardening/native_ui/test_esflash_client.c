#define _GNU_SOURCE
#include "esflash_client.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/reboot.h>
static unsigned assertions;
#define CHECK(x) do{++assertions;if(!(x)){printf("ESFLASH_FAILED line=%d\n",__LINE__);reboot(RB_POWER_OFF);_exit(1);}}while(0)
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 CHECK(x2d_flash_default_delay(1)==168000);CHECK(x2d_flash_default_delay(2)==160000);
 CHECK(x2d_flash_default_delay(3)==370000);CHECK(x2d_flash_default_delay(4)==370000);
 X2dFlashRequest req={X2D_ESFLASH_MAGIC,1,1,1,168000};
 CHECK(x2d_flash_request_valid(&req,0));CHECK(!x2d_flash_request_valid(&req,1));
 req.profile=5;CHECK(!x2d_flash_request_valid(&req,0));req.profile=1;req.delay_us=5000001;
 CHECK(!x2d_flash_request_valid(&req,0));req.delay_us=0;CHECK(x2d_flash_request_valid(&req,0));
 req.sequence=0;CHECK(!x2d_flash_request_valid(&req,UINT32_MAX));
 int listener=socket(AF_UNIX,SOCK_DGRAM,0);CHECK(listener>=0);
 struct sockaddr_un addr={0};addr.sun_family=AF_UNIX;memcpy(addr.sun_path+1,X2D_ESFLASH_SOCKET,sizeof X2D_ESFLASH_SOCKET-1);
 CHECK(!bind(listener,(struct sockaddr*)&addr,offsetof(struct sockaddr_un,sun_path)+sizeof X2D_ESFLASH_SOCKET));
 X2dFlashClient s;x2d_flash_client_init(&s);CHECK(s.fd==-1&&!s.ready);CHECK(!x2d_flash_client_set(&s,1,168000,0));
 x2d_flash_client_poll(&s,0);struct sockaddr_un peer={0};socklen_t peer_len=sizeof peer;
 CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.operation==0);
 X2dFlashReply reply={X2D_ESFLASH_MAGIC,0,0,168000,0,0,0,0,0,0,0};
 CHECK(x2d_flash_reply_valid(&reply));CHECK(x2d_flash_error_code(0)==0);
 CHECK(x2d_flash_error_code("helper_init")==3);CHECK(x2d_flash_error_code("perf_event_open")==24);CHECK(x2d_flash_error_code("unlisted")==255);
 X2dFlashReply diagnostic=reply;diagnostic.fault=1;diagnostic.error_code=3;diagnostic.error_no=13;CHECK(x2d_flash_reply_valid(&diagnostic));
 diagnostic.error_code=0;CHECK(!x2d_flash_reply_valid(&diagnostic));diagnostic.error_code=46;CHECK(!x2d_flash_reply_valid(&diagnostic));
 diagnostic.error_code=255;CHECK(x2d_flash_reply_valid(&diagnostic));diagnostic.error_no=4096;CHECK(!x2d_flash_reply_valid(&diagnostic));
 diagnostic=reply;diagnostic.magic=0x58454631u;CHECK(!x2d_flash_reply_valid(&diagnostic));
 CHECK(sendto(listener,&reply,sizeof reply,0,(struct sockaddr*)&peer,peer_len)==sizeof reply);x2d_flash_client_poll(&s,1);CHECK(s.ready&&!s.pending&&!s.state.requested);
 CHECK(x2d_flash_client_set(&s,2,160000,2));CHECK(!x2d_flash_client_set(&s,0,0,3));
 CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.profile==2&&req.sequence==1);
 reply.sequence=1;reply.profile=2;reply.delay_us=160000;reply.requested=1;reply.sampling=1;
 CHECK(sendto(listener,&reply,sizeof reply,0,(struct sockaddr*)&peer,peer_len)==sizeof reply);x2d_flash_client_poll(&s,4);CHECK(s.ready&&s.state.requested);
 /* 页面/GUI 连接关闭不发送 disable，也不改变后台状态。 */
 x2d_flash_client_close(&s);CHECK(recv(listener,&req,sizeof req,MSG_DONTWAIT)<0&&errno==EAGAIN);
 x2d_flash_client_poll(&s,1005);peer_len=sizeof peer;
 CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.operation==0);
 CHECK(sendto(listener,&reply,sizeof reply,0,(struct sockaddr*)&peer,peer_len)==sizeof reply);x2d_flash_client_poll(&s,1006);CHECK(s.ready&&s.state.requested&&s.state.sequence==1);
 /* 设置已执行但回复丢失：下一次仅查询，不能重发。 */
 CHECK(x2d_flash_client_set(&s,4,370000,1007));CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.sequence==2);
 reply.sequence=2;reply.profile=4;reply.delay_us=370000;
 x2d_flash_client_poll(&s,1507);CHECK(s.fd==-1&&!s.ready);
 x2d_flash_client_poll(&s,2507);peer_len=sizeof peer;CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.operation==0);
 reply.fault=1;reply.error_code=24;reply.error_no=13;
 CHECK(sendto(listener,&reply,sizeof reply,0,(struct sockaddr*)&peer,peer_len)==sizeof reply);x2d_flash_client_poll(&s,2508);CHECK(s.ready&&s.state.profile==4&&s.state.fault&&s.state.error_code==24&&s.state.error_no==13);
 reply.fault=0;reply.error_code=reply.error_no=0;
 x2d_flash_client_poll(&s,3509);CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.operation==0);
 reply.profile=5;CHECK(sendto(listener,&reply,sizeof reply,0,(struct sockaddr*)&peer,peer_len)==sizeof reply);x2d_flash_client_poll(&s,3510);CHECK(s.fd==-1&&!s.ready);
 /* A periodic query must allow one explicit setting, without replay. */
 x2d_flash_client_poll(&s,4510);peer_len=sizeof peer;
 CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.operation==0);
 reply.profile=4;CHECK(sendto(listener,&reply,sizeof reply,0,(struct sockaddr*)&peer,peer_len)==sizeof reply);
 x2d_flash_client_poll(&s,4511);CHECK(s.ready&&!s.pending);
 x2d_flash_client_poll(&s,5511);CHECK(s.pending&&!s.pending_operation);
 CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.operation==0);
 CHECK(x2d_flash_client_set(&s,1,168000,5512)&&s.queued);
 CHECK(!x2d_flash_client_set(&s,2,160000,5513));
 CHECK(recv(listener,&req,sizeof req,MSG_DONTWAIT)<0&&errno==EAGAIN);
 CHECK(sendto(listener,&reply,sizeof reply,0,(struct sockaddr*)&peer,peer_len)==sizeof reply);
 x2d_flash_client_poll(&s,5514);CHECK(!s.queued&&s.pending&&s.pending_operation==1);
 CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.operation==1&&req.sequence==3&&req.profile==1);
 x2d_flash_client_poll(&s,6014);CHECK(s.fd==-1&&!s.queued);
 x2d_flash_client_poll(&s,7014);peer_len=sizeof peer;
 CHECK(recvfrom(listener,&req,sizeof req,0,(struct sockaddr*)&peer,&peer_len)==sizeof req&&req.operation==0);
 CHECK(x2d_flash_client_set(&s,1,168000,7015)==0);
 close(listener);printf("ESFLASH_PASSED assertions=%u\n",assertions);reboot(RB_POWER_OFF);return 0;
}
