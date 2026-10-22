#include "effect_service.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

int x2d_effect_parse(const char*data,size_t size){
 if(!data||size<19||size>=4096||memchr(data,0,size))return -1;
 if(memcmp(data,"HTTP/1.1 200 ",13)&&memcmp(data,"HTTP/1.0 200 ",13))return -1;
 size_t at=0;while(at+1<size&&(data[at]!='\r'||data[at+1]!='\n'))++at;
 if(at+1>=size)return -1;at+=2;int length_seen=0;
 while(at+1<size){
  if(data[at]=='\r'&&data[at+1]=='\n'){at+=2;return length_seen&&size-at==1&&data[at]>='0'&&data[at]<='2'?data[at]-'0':-1;}
  size_t end=at;while(end+1<size&&(data[end]!='\r'||data[end+1]!='\n'))++end;
  if(end+1>=size||end==at||data[at]==' '||data[at]=='\t')return -1;
  const char*colon=memchr(data+at,':',end-at);if(!colon)return -1;
  size_t name=(size_t)(colon-data)-at,value=(size_t)(colon-data)+1;
  while(value<end&&(data[value]==' '||data[value]=='\t'))++value;
  size_t tail=end;while(tail>value&&(data[tail-1]==' '||data[tail-1]=='\t'))--tail;
  if(name==14&&!strncasecmp(data+at,"Content-Length",14)){
   if(length_seen||tail-value!=1||data[value]!='1')return -1;length_seen=1;
  }else if(name==17&&!strncasecmp(data+at,"Transfer-Encoding",17))return -1;
  at=end+2;
 }
 return -1;
}
static void close_socket(X2dEffectService*s){if(s->fd>=0)close(s->fd);s->fd=-1;s->saving=0;s->connecting=0;}
static void failed(X2dEffectService*s,uint64_t now){
 int write=s->requested>=0;close_socket(s);s->ready=0;s->error=write?2:1;
 if(write)s->attempts=0;s->next_read=now+300;
}
void x2d_effect_init(X2dEffectService*s){if(!s)return;memset(s,0,sizeof *s);s->fd=-1;s->requested=-1;s->initial=1;}
void x2d_effect_destroy(X2dEffectService*s){if(!s)return;close_socket(s);s->enabled=0;s->ready=0;}
static void begin(X2dEffectService*s,int mode,uint64_t now){
 s->requested=mode;s->sent=s->received=0;s->deadline=now+3000;s->saving=1;
 int n=mode<0?snprintf(s->request,sizeof s->request,"GET /mode HTTP/1.1\r\nHost: 127.0.0.1:38491\r\nConnection: close\r\n\r\n"):
  snprintf(s->request,sizeof s->request,"POST /mode/%d HTTP/1.1\r\nHost: 127.0.0.1:38491\r\nConnection: close\r\nContent-Length: 0\r\n\r\n",mode);
 if(n<0||(size_t)n>=sizeof s->request){failed(s,now);return;}s->request_size=(size_t)n;
 s->fd=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
 if(s->fd<0){failed(s,now);return;}
 struct sockaddr_in address;memset(&address,0,sizeof address);address.sin_family=AF_INET;address.sin_port=htons(38491);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 int rc=connect(s->fd,(const struct sockaddr*)&address,sizeof address);
 if(rc&&errno!=EINPROGRESS){failed(s,now);return;}s->connecting=rc!=0;
}
void x2d_effect_set_enabled(X2dEffectService*s,int enabled,uint64_t now){
 if(!s||s->enabled==!!enabled)return;s->enabled=!!enabled;
 if(!s->enabled){if(s->saving)failed(s,now);return;}
 if(!s->ready&&s->initial){s->initial=0;begin(s,-1,now);}else if(!s->ready)s->next_read=now+300;
}
int x2d_effect_select(X2dEffectService*s,int mode,uint64_t now){
 if(!s||!s->enabled||!s->ready||s->saving||mode<0||mode>2)return 0;
 begin(s,mode,now);return 1;
}
void x2d_effect_poll(X2dEffectService*s,uint64_t now){
 if(!s||!s->enabled)return;
 if(!s->saving){
  if(!s->ready&&s->attempts<34&&now>=s->next_read){++s->attempts;begin(s,-1,now);}
  return;
 }
 if(now>=s->deadline){failed(s,now);return;}
 if(s->connecting){
  struct pollfd ready={s->fd,POLLOUT,0};int n=poll(&ready,1,0);
  if(n<0){if(errno!=EINTR)failed(s,now);return;}if(!n)return;
  int error=0;socklen_t length=sizeof error;
  if(getsockopt(s->fd,SOL_SOCKET,SO_ERROR,&error,&length)||error){failed(s,now);return;}s->connecting=0;
 }
 if(s->sent<s->request_size){
  ssize_t n=send(s->fd,s->request+s->sent,s->request_size-s->sent,MSG_NOSIGNAL);
  if(n<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)failed(s,now);return;}
  if(!n){failed(s,now);return;}s->sent+=(size_t)n;if(s->sent<s->request_size)return;
 }
 /* 每次GUI轮询最多读取四次，避免忙服务占用主线程。 */
 for(unsigned i=0;i<4;++i){
  if(s->received==sizeof s->response){failed(s,now);return;}
  ssize_t n=recv(s->fd,s->response+s->received,sizeof s->response-s->received,0);
  if(n<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)failed(s,now);return;}
  if(!n){int mode=x2d_effect_parse(s->response,s->received);if(mode<0){failed(s,now);return;}close_socket(s);s->mode=mode;s->ready=1;s->error=0;return;}
  s->received+=(size_t)n;
 }
}
