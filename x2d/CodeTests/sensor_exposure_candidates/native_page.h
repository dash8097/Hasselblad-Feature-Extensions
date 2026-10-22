/* 包内服务：固定本地协议，设置仅接受主 camera-gui 的内核凭据。 */
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include "esflash_protocol.h"
#include "flash_core.h"
static int bridge_failed,front_fd=-1;
static const char *sampling_error;
static int sampling_errno;
static unsigned sampling_retryable,thread_refreshes,sampled_threads,requested_enabled,native_profile;
static X2dFlashReply native_reply;
static int page_identity(void){return 1;}
static int front_init(void){return 0;}
static int front_poll(void){return 0;}
static int front_console(const char *s){(void)s;return 0;}
static const char*front_marker(const char*s,const char*prefix){size_t n=strlen(prefix);return strncmp(s,prefix,n)?0:s+n;}
static unsigned native_find_service(void){
 int fd=open("/proc",0x84000);if(fd<0)return 0;unsigned found=0;unsigned char data[4096];
 for(;;){long n=syscall(61,fd,data,sizeof data);if(n<0&&*__errno()==4)continue;if(n<0){found=0;break;}if(!n)break;
  for(long at=0;at<n;){if(n-at<20){found=0;goto done;}unsigned len=(unsigned)sensor_le(data+at+16,2);if(len<20||len>(unsigned)(n-at)){found=0;goto done;}
   char*name=(char*)data+at+19;if(!memchr(name,0,len-19)){found=0;goto done;}unsigned pid=0;
   if(number(name,&pid)&&pid){char path[80],target[128];snprintf(path,sizeof path,"/proc/%u/exe",pid);long count=readlink(path,target,sizeof target-1);
    if(count>0&&count<(long)sizeof target-1){target[count]=0;if(equal(target,"/system/bin/camera-service")){if(found){found=0;goto done;}found=pid;}}}
   at+=len;
  }
 }done:close(fd);return found;
}
static int native_peer(struct ucred cred){
 if(cred.uid!=0||cred.pid<=0)return 0;
 char path[80],target[256];snprintf(path,sizeof path,"/proc/%u/exe",(unsigned)cred.pid);long n=readlink(path,target,sizeof target-1);
 if(n<=0||n>=(long)sizeof target-1)return 0;target[n]=0;if(!equal(target,"/system/bin/camera-gui"))return 0;
 snprintf(path,sizeof path,"/proc/%u/cmdline",(unsigned)cred.pid);n=file_read(path,target,sizeof target);
 if(n<=0||n==(long)sizeof target)return 0;for(long at=0;at<n;){const char*s=target+at;const char*end=memchr(s,0,(size_t)(n-at));if(!end)return 0;if(equal(s,"--imagetest")||equal(s,"--touchtest")||equal(s,"--sutest")||equal(s,"--lenstest"))return 0;at+=(long)(end-s)+1;}
 return 1;
}
static int page_initialize(void){
 int fd=socket(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(fd<0)return -1;
 struct sockaddr_un a={0};a.sun_family=AF_UNIX;memcpy(a.sun_path+1,X2D_ESFLASH_SOCKET,sizeof X2D_ESFLASH_SOCKET-1);
 int credentials=1;
 if(setsockopt(fd,SOL_SOCKET,SO_PASSCRED,&credentials,sizeof credentials)||bind(fd,(struct sockaddr*)&a,offsetof(struct sockaddr_un,sun_path)+sizeof X2D_ESFLASH_SOCKET)){close(fd);return -1;}
 return fd;
}
static void page_status(const FlashCore*g,const char*reason,unsigned mask,unsigned sampling,unsigned helper,uint64_t actual){
 (void)reason;(void)mask;(void)helper;(void)actual;
 native_reply=(X2dFlashReply){X2D_ESFLASH_MAGIC,g->sequence,native_profile,g->delay_us,requested_enabled,sampling,sampling_error?1u:0u,g->attempts,g->skipped,x2d_flash_error_code(sampling_error),sampling_error?(sampling_errno>=0&&sampling_errno<=4095?(unsigned)sampling_errno:4095u):0u};
}
