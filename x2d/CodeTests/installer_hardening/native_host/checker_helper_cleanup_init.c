#define _GNU_SOURCE
/* 仅在无网络、无USB、无主机目录的 ARM64 QEMU 客体内运行。 */
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static unsigned checks, cases;
static const char *previous="/system/bin/x2d-native-setup.previous.0123456789abcdef";
static char stage[128], final[160];
static void check(int v,const char *why){
 ++checks;
 if(!v){printf("HELPER_CHECK_FAILED case=%u reason=%s errno=%d\n",cases,why,errno);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}
}
static void dir(const char *p){check(!mkdir(p,0700)||errno==EEXIST,p);}
static int exists(const char *p){struct stat s;return !lstat(p,&s);}
static void ro(int readonly){check(!mount(NULL,"/system",NULL,MS_REMOUNT|(readonly?MS_RDONLY:0),NULL),"system remount");}
static int contains(const char *p,const char *s){
 char b[16384];int fd=open(p,O_RDONLY);check(fd>=0,"receipt open");
 ssize_t n=read(fd,b,sizeof b-1);check(n>=0&&n<(ssize_t)sizeof b-1,"receipt length");close(fd);b[n]=0;return strstr(b,s)!=NULL;
}
static void next_stage(void){
 ++cases;snprintf(stage,sizeof stage,"/data/local/tmp/x2d-tidy-%08x",cases);
 snprintf(final,sizeof final,"%s/final",stage);dir(stage);
}
static void fixture(unsigned index,int mutation){
 ro(0);check(!exists(previous)||!unlink(previous),"remove guest fixture");
 char p[64];snprintf(p,sizeof p,"/fixtures/old-%u",index);
 int in=open(p,O_RDONLY),out=open(previous,O_WRONLY|O_CREAT|O_EXCL,0755);
 check(in>=0&&out>=0,"fixture open");
 char b[65536];ssize_t n;int first=1;
 while((n=read(in,b,sizeof b))>0){
  if(first&&mutation){b[0]^=1;first=0;}
  check(write(out,b,(size_t)n)==n,"fixture copy");
 }
 check(n==0,"fixture read");close(in);close(out);check(!chmod(previous,0755),"fixture mode");ro(1);
}
static void unknown(void){
 ro(0);check(!exists(previous)||!unlink(previous),"remove previous fixture");
 int fd=open(previous,O_WRONLY|O_CREAT|O_EXCL,0755);check(fd>=0,"unknown open");
 check(write(fd,"unregistered",12)==12,"unknown write");close(fd);check(!chmod(previous,0755),"unknown mode");ro(1);
}
static void run(int retained){
 next_stage();
 pid_t p=fork();check(p>=0,"fork");
 if(!p){execl("/flow","/flow","data-cleanup","uninstalled",stage,"1234567890abcdef",NULL);_exit(127);}
 int status;check(waitpid(p,&status,0)==p&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"cleanup return");
 check(contains(final,"final_rc=0\n")&&contains(final,"restore_rc=0\n"),"receipt rc");
 check(contains(final,retained?"outcome=deferred-retained\n":"outcome=complete\n"),"semantic outcome");
 check(exists(previous)==retained,"previous presence");
 struct statvfs fs;check(!statvfs("/system",&fs)&&(fs.f_flag&ST_RDONLY),"readonly restored");
 check(!exists("/tmp/x2d-multishot-active")&&!exists("/tmp/x2d-data-cleanup-lock"),"own locks released");
 printf("HELPER_CASE_PASS case=%u retained=%d\n",cases,retained);
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 check(!mount("proc","/proc","proc",0,NULL),"proc");
 check(!mount("tmpfs","/tmp","tmpfs",0,NULL),"tmp");
 check(!mount("tmpfs","/data","tmpfs",0,NULL),"data");
 check(!mount("tmpfs","/system","tmpfs",0,NULL),"system");
 dir("/data/local");dir("/data/local/tmp");dir("/system/bin");dir("/system/etc");
 /* 精确旧二进制删除；单字节改动必须保留；被父进程持有则延期，释放后可清理。 */
 for(unsigned i=0;i<2;i++){
  fixture(i,0);run(0);
  fixture(i,1);run(1);
  fixture(i,0);int held=open(previous,O_RDONLY|O_CLOEXEC);check(held>=0,"held fixture");
  run(1);check(!close(held),"close held");run(0);
 }
 unknown();run(1);
 printf("HELPER_CLEANUP_ALL_PASSED cases=%u checks=%u\n",cases,checks);
 sync();reboot(RB_POWER_OFF);return 0;
}
