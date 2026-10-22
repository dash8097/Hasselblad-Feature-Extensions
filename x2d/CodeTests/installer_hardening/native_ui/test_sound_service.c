#define _GNU_SOURCE
#include "sound_service.h"
#include <arpa/inet.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned assertions;
#define CHECK(x) do{++assertions;if(!(x)){printf("SOUND_SERVICE_FAILED line=%d\n",__LINE__);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}while(0)
static const char*reply="HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\n";
static pid_t server(int uncertain){
 int fd=socket(AF_INET,SOCK_STREAM,0),yes=1;CHECK(fd>=0);CHECK(!setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes));
 struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_port=htons(38491);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 CHECK(!bind(fd,(struct sockaddr*)&a,sizeof a));CHECK(!listen(fd,4));pid_t child=fork();CHECK(child>=0);
 if(!child){alarm(10);unsigned count=uncertain==1?3:2;for(unsigned i=0;i<count;++i){int c=accept(fd,NULL,NULL);if(c<0)_exit(2);char request[200]={0};size_t at=0;
   while(at<sizeof request-1&&!strstr(request,"\r\n\r\n")){ssize_t n=read(c,request+at,sizeof request-1-at);if(n<=0)_exit(3);at+=(size_t)n;}
   if(i==1){if(strncmp(request,"GET /play HTTP/",15))_exit(4);}else if(strncmp(request,"GET /ready HTTP/",16))_exit(5);
   if(uncertain==2&&i==1)usleep(500000);
   if(!(uncertain&&i==1)){size_t n=strlen(reply);if(write(c,reply,11)!=11)_exit(6);usleep(1000);if(write(c,reply+11,n-11)!=(ssize_t)(n-11))_exit(7);if(write(c,"OK",2)!=2)_exit(8);}close(c);
  }close(fd);_exit(0);
 }close(fd);return child;
}
static void spin(X2dSoundService*s,uint64_t*now){for(unsigned i=0;i<400&&s->saving;++i){*now+=5;x2d_sound_poll(s,*now);usleep(1000);}CHECK(!s->saving);}
static void finish(pid_t child){int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);}
static void parse_tests(void){
 const char*good="HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";CHECK(x2d_sound_parse(good,strlen(good))==0);
 const char*bad[]={"HTTP/1.1 503 Error\r\nContent-Length: 2\r\n\r\nOK","HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\n0","HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\nOK","HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 2\r\n\r\nOK","HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOKx","HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nNO"};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++)CHECK(x2d_sound_parse(bad[i],strlen(bad[i]))<0);
 CHECK(x2d_sound_parse(NULL,0)<0);
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);mount("proc","/proc","proc",0,NULL);
 int net=socket(AF_INET,SOCK_DGRAM,0);CHECK(net>=0);struct ifreq req={0};strcpy(req.ifr_name,"lo");CHECK(!ioctl(net,SIOCGIFFLAGS,&req));req.ifr_flags|=IFF_UP;CHECK(!ioctl(net,SIOCSIFFLAGS,&req));close(net);
 parse_tests();
 for(int uncertain=0;uncertain<2;++uncertain){pid_t child=server(uncertain);uint64_t now=0;X2dSoundService s;x2d_sound_init(&s);CHECK(s.fd==-1&&!s.enabled&&!s.saving);CHECK(!x2d_sound_play(&s,now));x2d_sound_set_enabled(&s,1,now);spin(&s,&now);CHECK(s.ready);CHECK(x2d_sound_play(&s,now));CHECK(!x2d_sound_play(&s,now));spin(&s,&now);
  if(uncertain){CHECK(!s.ready&&s.error==2&&s.attempts==0);CHECK(!x2d_sound_play(&s,now));now+=301;x2d_sound_poll(&s,now);spin(&s,&now);CHECK(s.attempts==1);}
  CHECK(s.ready&&!s.error);finish(child);x2d_sound_destroy(&s);CHECK(s.fd==-1&&!s.enabled);
 }
 {pid_t child=server(2);uint64_t now=0;X2dSoundService timed;x2d_sound_init(&timed);x2d_sound_set_enabled(&timed,1,now);spin(&timed,&now);CHECK(timed.ready);CHECK(x2d_sound_play(&timed,now));
  for(unsigned i=0;i<200&&timed.sent<timed.request_size;++i){now+=5;x2d_sound_poll(&timed,now);usleep(1000);}
  CHECK(timed.saving&&timed.sent==timed.request_size);now=timed.deadline;x2d_sound_poll(&timed,now);CHECK(!timed.saving&&!timed.ready&&timed.error==2&&timed.fd==-1);x2d_sound_destroy(&timed);finish(child);
 }
 X2dSoundService s;x2d_sound_init(&s);uint64_t now=0;x2d_sound_set_enabled(&s,1,now);
 for(unsigned i=0;i<200;++i){now+=301;x2d_sound_poll(&s,now);usleep(100);}
 CHECK(s.attempts==34&&!s.ready&&!s.saving);int attempts=s.attempts;now+=10000;x2d_sound_poll(&s,now);CHECK(s.attempts==attempts);
 x2d_sound_destroy(&s);x2d_sound_init(&s);x2d_sound_set_enabled(&s,1,0);x2d_sound_set_enabled(&s,0,1);CHECK(!s.enabled&&!s.saving&&s.fd==-1);x2d_sound_poll(&s,100000);CHECK(!s.saving&&s.attempts==0);
 printf("SOUND_SERVICE_PASSED assertions=%u\n",assertions);reboot(RB_POWER_OFF);return 0;
}
