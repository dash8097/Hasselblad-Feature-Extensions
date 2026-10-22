#define _GNU_SOURCE
#include "passive_output.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static unsigned checks;
#define CK(x) do{++checks;if(!(x)){printf("PASSIVE_OUTPUT_FAIL line=%d checks=%u\n",__LINE__,checks);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}while(0)
static uint64_t now(void){struct timespec t;CK(!clock_gettime(CLOCK_MONOTONIC,&t));return(uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static const char*file[2]={"/tmp/x2d-pixel-overclock/playback-live-status.json","/tmp/x2d-pixel-overclock/ui.txt"};
static void send(X2dPassiveOutput*w,int k,const char*s,size_t n,uint64_t deadline,int error){uint64_t old=x2d_passive_processed(w,k);int got=0;for(unsigned i=0;i<1000&&!got;i++){got=x2d_passive_publish(w,k,s,n,deadline);if(!got)usleep(1000);}CK(got);for(unsigned i=0;i<1000&&x2d_passive_processed(w,k)==old;i++)usleep(1000);CK(x2d_passive_processed(w,k)==old+1);CK(x2d_passive_error(w,k)==error);}
static void verify(int k,const char*s,size_t n){char b[32769];int fd=open(file[k],O_RDONLY);CK(fd>=0);ssize_t z=read(fd,b,sizeof b);CK(z==(ssize_t)n&&!memcmp(b,s,n));CK(!close(fd));}
int main(void){setvbuf(stdout,NULL,_IONBF,0);CK(!mount("proc","/proc","proc",0,NULL));CK(!mkdir("/tmp",0700));X2dPassiveOutput*w=x2d_passive_create();CK(w);send(w,0,"{}",2,now()+5000,ENOENT);CK(access("/tmp/x2d-pixel-overclock",F_OK));CK(!mkdir("/tmp/x2d-pixel-overclock",0700));
 for(unsigned i=0;i<256;i++){char data[80];int n=snprintf(data,sizeof data,"{\"version\":3,\"sequence\":%u}",i);send(w,0,data,(size_t)n,now()+5000,0);verify(0,data,(size_t)n);send(w,1,"NATIVE_UI_ATTACHING\n",20,now()+5000,0);verify(1,"NATIVE_UI_ATTACHING\n",20);}
 CK(x2d_passive_written(w,0)==256&&x2d_passive_written(w,1)==256);CK(!x2d_passive_publish(w,2,"x",1,now()+1000));CK(!x2d_passive_publish(w,1,"a\0b",3,now()+1000));char large[32769];memset(large,'x',sizeof large);CK(!x2d_passive_publish(w,0,large,sizeof large,now()+1000));CK(!x2d_passive_publish(w,1,large,2049,now()+1000));send(w,1,large,2048,now()+1000,0);verify(1,large,2048);send(w,0,large,32768,now()+1000,0);verify(0,large,32768);
 send(w,0,"expired",7,1,ECANCELED);verify(0,large,32768);CK(!unlink(file[0]));CK(!symlink("/tmp/untouched",file[0]));send(w,0,"safe",4,now()+1000,EPERM);struct stat st;CK(!lstat(file[0],&st)&&S_ISLNK(st.st_mode));CK(access("/tmp/untouched",F_OK));CK(!unlink(file[0]));
 int fd=open(file[0],O_CREAT|O_WRONLY,0600);CK(fd>=0);CK(!close(fd));CK(!link(file[0],"/tmp/hard"));send(w,0,"safe",4,now()+1000,EPERM);CK(!unlink("/tmp/hard"));CK(!unlink(file[0]));CK(!mkfifo(file[0],0600));send(w,0,"safe",4,now()+1000,EPERM);CK(!unlink(file[0]));CK(!mkdir(file[0],0700));send(w,0,"safe",4,now()+1000,EPERM);CK(!rmdir(file[0]));
 CK(!chmod("/tmp/x2d-pixel-overclock",0777));send(w,0,"safe",4,now()+1000,EPERM);CK(!chmod("/tmp/x2d-pixel-overclock",0700));CK(!rename("/tmp/x2d-pixel-overclock","/tmp/moved"));CK(!symlink("/tmp/moved","/tmp/x2d-pixel-overclock"));send(w,0,"safe",4,now()+1000,ENOTDIR);CK(!unlink("/tmp/x2d-pixel-overclock"));CK(!rename("/tmp/moved","/tmp/x2d-pixel-overclock"));
 x2d_passive_stop(w);CK(!x2d_passive_publish(w,0,"x",1,now()+1000));int released=0;for(unsigned i=0;i<1000&&!released;i++){released=x2d_passive_release(w);if(!released)usleep(1000);}CK(released);printf("PASSIVE_OUTPUT_PASS assertions=%u rounds=256 actual_ARM_Linux=1 camera_operations=0\n",checks);reboot(RB_POWER_OFF);return 0;}
