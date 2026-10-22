#define _GNU_SOURCE
/* 仅在隔离 ARM64 QEMU 客体内使用；列表由已验证 manifest 生成。 */
#include <sys/mount.h>
#include <sys/mman.h>
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
#include "library_paths.h"
static unsigned checks, cases;
static char stage[128], final[160];
static void check(int v,const char *why){
 ++checks;
 if(!v){printf("LIBRARY_CHECK_FAILED case=%u reason=%s errno=%d\n",cases,why,errno);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}
}
static void dir(const char *p){check(!mkdir(p,0700)||errno==EEXIST,p);}
static int exists(const char *p){struct stat s;return !lstat(p,&s);}
static void ro(int readonly){check(!mount(NULL,"/system",NULL,MS_REMOUNT|(readonly?MS_RDONLY:0),NULL),"system remount");}
static void remove_guest(const char *p){check(!exists(p)||!unlink(p),"remove guest fixture");}
static int contains(const char *p,const char *s){
 char b[16384];int fd=open(p,O_RDONLY);check(fd>=0,"receipt open");
 ssize_t n=read(fd,b,sizeof b-1);check(n>=0&&n<(ssize_t)sizeof b-1,"receipt length");close(fd);b[n]=0;return strstr(b,s)!=NULL;
}
static void fixture(unsigned index,int mutation,const char *output){
 char p[64];snprintf(p,sizeof p,"/fixtures/library-%u",index);
 int in=open(p,O_RDONLY),out=open(output,O_WRONLY|O_CREAT|O_EXCL,0644);
 check(in>=0&&out>=0,"fixture open");
 char b[65536];ssize_t n;int first=1;
 while((n=read(in,b,sizeof b))>0){
  if(first&&mutation){b[0]^=1;first=0;}
  check(write(out,b,(size_t)n)==n,"fixture copy");
 }
 check(n==0,"fixture read");close(in);close(out);check(!chmod(output,0644),"fixture mode");
}
static void seed(int mutation){
 ro(0);for(unsigned i=0;i<LIBRARY_COUNT;i++){remove_guest(library_paths[i]);fixture(i,mutation,library_paths[i]);}ro(1);
}
static void clear(void){ro(0);for(unsigned i=0;i<LIBRARY_COUNT;i++)remove_guest(library_paths[i]);ro(1);}
static void run(int retained,int present){
 ++cases;snprintf(stage,sizeof stage,"/data/local/tmp/x2d-tidy-%08x",cases);
 snprintf(final,sizeof final,"%s/final",stage);dir(stage);
 pid_t p=fork();check(p>=0,"fork");
 if(!p){execl("/flow","/flow","data-cleanup","uninstalled",stage,"1234567890abcdef",NULL);_exit(127);}
 int status;check(waitpid(p,&status,0)==p&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"cleanup return");
 check(contains(final,"final_rc=0\n")&&contains(final,"restore_rc=0\n"),"receipt rc");
 check(contains(final,retained?"outcome=deferred-retained\n":"outcome=complete\n"),"semantic outcome");
 for(unsigned i=0;i<LIBRARY_COUNT;i++)check(exists(library_paths[i])==present,library_paths[i]);
 struct statvfs fs;check(!statvfs("/system",&fs)&&(fs.f_flag&ST_RDONLY),"readonly restored");
 check(!exists("/tmp/x2d-multishot-active")&&!exists("/tmp/x2d-data-cleanup-lock"),"own locks released");
 printf("LIBRARY_CASE_PASS case=%u retained=%d present=%d\n",cases,retained,present);
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 check(!mount("proc","/proc","proc",0,NULL),"proc");
 check(!mount("tmpfs","/tmp","tmpfs",0,NULL),"tmp");
 check(!mount("tmpfs","/data","tmpfs",0,NULL),"data");
 check(!mount("tmpfs","/system","tmpfs",0,NULL),"system");
 dir("/data/local");dir("/data/local/tmp");dir("/system/bin");dir("/system/etc");dir("/system/lib64");dir("/system/links");
 seed(0);run(0,0);
 seed(1);run(1,1);clear();
 ro(0);
 for(unsigned i=0;i<LIBRARY_COUNT;i++){char p[64];snprintf(p,sizeof p,"/fixtures/library-%u",i);check(!symlink(p,library_paths[i]),"create symlink");}
 ro(1);run(1,1);
 for(unsigned i=0;i<LIBRARY_COUNT;i++){char p[64];snprintf(p,sizeof p,"/fixtures/library-%u",i);check(exists(p),"symlink target retained");}
 clear();seed(0);ro(0);
 for(unsigned i=0;i<LIBRARY_COUNT;i++){char p[64];snprintf(p,sizeof p,"/system/links/library-%u",i);check(!link(library_paths[i],p),"create hardlink");}
 ro(1);run(1,1);ro(0);
 for(unsigned i=0;i<LIBRARY_COUNT;i++){char p[64];snprintf(p,sizeof p,"/system/links/library-%u",i);check(!unlink(p),"release hardlink");}
 ro(1);run(0,0);
 seed(0);int fds[LIBRARY_COUNT];
 for(unsigned i=0;i<LIBRARY_COUNT;i++){fds[i]=open(library_paths[i],O_RDONLY|O_CLOEXEC);check(fds[i]>=0,"hold fd");}
 run(1,1);for(unsigned i=0;i<LIBRARY_COUNT;i++)check(!close(fds[i]),"release fd");run(0,0);
 seed(0);void *maps[LIBRARY_COUNT];
 for(unsigned i=0;i<LIBRARY_COUNT;i++){int fd=open(library_paths[i],O_RDONLY|O_CLOEXEC);check(fd>=0,"map open");maps[i]=mmap(NULL,4096,PROT_READ,MAP_PRIVATE,fd,0);check(maps[i]!=MAP_FAILED,"hold mmap");close(fd);}
 run(1,1);for(unsigned i=0;i<LIBRARY_COUNT;i++)check(!munmap(maps[i],4096),"release mmap");run(0,0);
 ro(0);const char *unknown="/system/lib64/libx2d_checker_unregistered.so";fixture(0,0,unknown);ro(1);
 run(0,0);check(exists(unknown),"same hash at unknown path retained");
 printf("LIBRARY_CLEANUP_ALL_PASSED cases=%u libraries=%u checks=%u\n",cases,LIBRARY_COUNT,checks);
 sync();reboot(RB_POWER_OFF);return 0;
}
