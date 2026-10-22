#include "esflash_client.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
void x2d_flash_client_init(X2dFlashClient*s){memset(s,0,sizeof *s);s->fd=-1;}
void x2d_flash_client_close(X2dFlashClient*s){if(s->fd>=0)close(s->fd);s->fd=-1;s->ready=s->pending=0;s->queued=0;s->pending_operation=0;}
static void failed(X2dFlashClient*s,uint64_t now){x2d_flash_client_close(s);s->next=now+1000;}
static int request(X2dFlashClient*s,const X2dFlashRequest*r,uint64_t now){
 if(send(s->fd,r,sizeof *r,MSG_NOSIGNAL|MSG_DONTWAIT)!=(long)sizeof *r){failed(s,now);return 0;}
 s->pending=1;s->pending_operation=r->operation;s->deadline=now+500;return 1;
}
int x2d_flash_client_set(X2dFlashClient*s,unsigned profile,unsigned delay,uint64_t now){
 if(!s||!s->ready||s->queued||(s->pending&&s->pending_operation)||s->state.sequence==UINT32_MAX)return 0;
 X2dFlashRequest r={X2D_ESFLASH_MAGIC,1,s->state.sequence+1,profile,delay};
 if(!x2d_flash_request_valid(&r,s->state.sequence))return 0;
 if(s->pending){s->queued=1;s->queued_profile=profile;s->queued_delay=delay;return 1;}
 return request(s,&r,now); /* 回复丢失只查询状态，不自动重发设置。 */
}
void x2d_flash_client_poll(X2dFlashClient*s,uint64_t now){
 if(!s)return;
 if(s->fd<0){if(now<s->next)return;s->fd=socket(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(s->fd<0){failed(s,now);return;}
  /* Linux 自动分配抽象地址；连接后只接收固定服务端的回复。 */
  struct sockaddr_un local={0};local.sun_family=AF_UNIX;
  if(bind(s->fd,(struct sockaddr*)&local,offsetof(struct sockaddr_un,sun_path))){failed(s,now);return;}
  struct sockaddr_un a={0};a.sun_family=AF_UNIX;memcpy(a.sun_path+1,X2D_ESFLASH_SOCKET,sizeof X2D_ESFLASH_SOCKET-1);
  if(connect(s->fd,(struct sockaddr*)&a,offsetof(struct sockaddr_un,sun_path)+sizeof X2D_ESFLASH_SOCKET)){failed(s,now);return;}
  s->next=now;
 }
 if(s->pending){X2dFlashReply r;long n=recv(s->fd,&r,sizeof r,MSG_DONTWAIT|MSG_TRUNC);
  if(n<0&&(errno==EAGAIN||errno==EINTR)){if(now>=s->deadline)failed(s,now);return;}
  if(n!=(long)sizeof r||!x2d_flash_reply_valid(&r)){failed(s,now);return;}
  s->state=r;s->ready=1;s->pending=0;s->pending_operation=0;s->next=now+1000;
  /* One explicit intent may wait for an in-flight read. Clear before send:
     a lost reply/reconnect never replays a setting. */
  if(s->queued){unsigned profile=s->queued_profile,delay=s->queued_delay;s->queued=0;
   if(!x2d_flash_client_set(s,profile,delay,now)){failed(s,now);return;}
  }
 }
 if(!s->pending&&now>=s->next){X2dFlashRequest r={X2D_ESFLASH_MAGIC,0,0,0,0};request(s,&r,now);}
}
