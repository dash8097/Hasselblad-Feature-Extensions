/* 仅直编候选启用。空闲时阻塞accept，无定时刷新或唤醒相机。 */
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>
#include "quarter_decision_wire.h"
static void *qn_decision_worker(void *arg){
 int listener=(int)(intptr_t)arg;
 for(;;){
  int client=accept4(listener,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK);
  if(client<0){if(errno==EINTR)continue;break;}
  struct pollfd p={client,POLLIN,0};QdDecision q{};
  if(poll(&p,1,10)>0&&recv(client,&q,sizeof q,MSG_TRUNC)==sizeof q&&qn_decision_reply(&q)){
   (void)send(client,&q,sizeof q,MSG_NOSIGNAL);
  }
  close(client);
 }
 close(listener);return nullptr;
}
static int qn_start_decisions(){
 int fd=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0);if(fd<0)return 0;
 struct sockaddr_un address{};address.sun_family=AF_UNIX;
 memcpy(address.sun_path,QD_DECISION_SOCKET,sizeof QD_DECISION_SOCKET);
 /* 清理的仅是本组件的旧socket；拒绝替换其它类型文件。 */
 struct stat st;
 if(!lstat(QD_DECISION_SOCKET,&st)){
  if(!S_ISSOCK(st.st_mode)){close(fd);return 0;}
  int probe=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
  if(probe<0){close(fd);return 0;}
  int rc=connect(probe,(struct sockaddr*)&address,sizeof address),why=errno;close(probe);
  if(!rc||why!=ECONNREFUSED||unlink(QD_DECISION_SOCKET)){close(fd);return 0;}
 }
 if(bind(fd,(struct sockaddr*)&address,sizeof address)||chmod(QD_DECISION_SOCKET,0600)||listen(fd,8)){
  close(fd);return 0;
 }
 pthread_t worker;
 if(pthread_create(&worker,nullptr,qn_decision_worker,(void*)(intptr_t)fd)){
  close(fd);unlink(QD_DECISION_SOCKET);return 0;
 }
 pthread_detach(worker);return 1;
}
