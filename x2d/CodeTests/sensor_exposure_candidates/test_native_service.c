#define _GNU_SOURCE
#include "esflash_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <fcntl.h>
static unsigned assertions;
#define CHECK(x) do{++assertions;if(!(x)){printf("FLASH_SERVICE_FAILED line=%d\n",__LINE__);fflush(stdout);exit(1);}}while(0)
static int connect_service(void){
 int fd=socket(AF_UNIX,SOCK_DGRAM,0);CHECK(fd>=0);struct timeval tv={2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);
 struct sockaddr_un local={0};local.sun_family=AF_UNIX;CHECK(!bind(fd,(struct sockaddr*)&local,offsetof(struct sockaddr_un,sun_path)));
 struct sockaddr_un a={0};a.sun_family=AF_UNIX;memcpy(a.sun_path+1,X2D_ESFLASH_SOCKET,sizeof X2D_ESFLASH_SOCKET-1);
 for(unsigned i=0;i<100;i++){if(!connect(fd,(struct sockaddr*)&a,offsetof(struct sockaddr_un,sun_path)+sizeof X2D_ESFLASH_SOCKET))return fd;usleep(50000);}CHECK(0);return -1;
}
static void client(int unauthorized){
 int fd=connect_service();X2dFlashRequest q={X2D_ESFLASH_MAGIC,0,0,0,0};X2dFlashReply r;
 CHECK(send(fd,&q,sizeof q,MSG_NOSIGNAL)==sizeof q);long n=recv(fd,&r,sizeof r,0);
 if(unauthorized){CHECK(n<=0);close(fd);return;}
 CHECK(n==sizeof r&&r.magic==X2D_ESFLASH_MAGIC&&!r.requested&&!r.sampling&&!r.profile);
 /* 同一正常发送者的短包、超长包、非法档位不改变状态。 */
 CHECK(send(fd,&q,sizeof q-1,MSG_NOSIGNAL)==sizeof q-1);CHECK(recv(fd,&r,sizeof r,0)<=0);
 unsigned char oversized[sizeof q+1];memcpy(oversized,&q,sizeof q);oversized[sizeof q]=0;
 CHECK(send(fd,oversized,sizeof oversized,MSG_NOSIGNAL)==sizeof oversized);CHECK(recv(fd,&r,sizeof r,0)<=0);
 q.operation=1;q.sequence=r.sequence+1;q.profile=5;
 CHECK(send(fd,&q,sizeof q,MSG_NOSIGNAL)==sizeof q);CHECK(recv(fd,&r,sizeof r,0)<=0);
 q=(X2dFlashRequest){X2D_ESFLASH_MAGIC,0,0,0,0};CHECK(send(fd,&q,sizeof q,MSG_NOSIGNAL)==sizeof q);
 CHECK(recv(fd,&r,sizeof r,0)==sizeof r&&!r.requested&&!r.sampling&&!r.profile);
 unsigned seq=r.sequence;q=(X2dFlashRequest){X2D_ESFLASH_MAGIC,1,seq+1,0,180000};
 CHECK(send(fd,&q,sizeof q,MSG_NOSIGNAL)==sizeof q);CHECK(recv(fd,&r,sizeof r,0)==sizeof r&&r.sequence==seq+1&&r.delay_us==180000&&!r.requested);
 CHECK(send(fd,&q,sizeof q,MSG_NOSIGNAL)==sizeof q);CHECK(recv(fd,&r,sizeof r,0)<=0);close(fd);
 fd=connect_service();q.operation=0;CHECK(send(fd,&q,sizeof q,MSG_NOSIGNAL)==sizeof q);CHECK(recv(fd,&r,sizeof r,0)==sizeof r&&r.delay_us==180000&&!r.requested);close(fd);
 printf("FLASH_AUTHORIZED_CLIENT_PASSED assertions=%u\n",assertions);
}
int main(int argc,char**argv){
 setvbuf(stdout,NULL,_IONBF,0);alarm(25);
 if(argc>1){client(!strcmp(argv[1],"unauthorized")||!strcmp(argv[1],"--touchtest"));return 0;}
 mount("proc","/proc","proc",0,NULL);
 pid_t backend=fork();CHECK(backend>=0);if(!backend){execl("/system/bin/x2d-esflash","x2d-esflash",NULL);_exit(90);}
 pid_t child=fork();CHECK(child>=0);if(!child){execl("/system/bin/camera-gui","camera-gui","test",NULL);_exit(91);}
 int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 child=fork();CHECK(child>=0);if(!child){execl("/init","init","unauthorized",NULL);_exit(92);}
 CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 child=fork();CHECK(child>=0);if(!child){execl("/system/bin/camera-gui","camera-gui","--touchtest",NULL);_exit(93);}
 CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 CHECK(!kill(backend,SIGTERM));CHECK(waitpid(backend,&status,0)==backend&&WIFEXITED(status)&&!WEXITSTATUS(status));
 /* Exact production helper must be inert in both a foreign process and a
    camera-service pathname with the wrong official executable digest. */
 child=fork();CHECK(child>=0);if(!child){execl("/system/bin/flash-helper-probe","/system/bin/flash-helper-probe",NULL);_exit(94);}
 CHECK(waitpid(child,&status,0)==child);printf("PARENT_FOREIGN_PROBE_STATUS=%d\n",status);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
 CHECK(!unlink("/system/bin/camera-service"));int input=open("/system/bin/flash-helper-probe",O_RDONLY),output=open("/system/bin/camera-service",O_WRONLY|O_CREAT|O_EXCL,0755);CHECK(input>=0&&output>=0);
 char buffer[4096];ssize_t n;while((n=read(input,buffer,sizeof buffer))>0)CHECK(write(output,buffer,(size_t)n)==n);CHECK(n==0&&!close(input)&&!close(output));
 child=fork();CHECK(child>=0);if(!child){execl("/system/bin/camera-service","/system/bin/camera-service",NULL);_exit(95);}
 CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 puts("FLASH_SERVICE_PASSED exact_backend=1 enable_requests=0 parent_negative_loaders=2");reboot(RB_POWER_OFF);return 0;
}
