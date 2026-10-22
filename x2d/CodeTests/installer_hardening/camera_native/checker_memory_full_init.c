#define _GNU_SOURCE
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static unsigned cases,checks;
static char job[256];
static void ck(int b,const char*s){checks++;if(!b){printf("MEMORY_FULL_FAIL case=%u check=%u errno=%d %s\n",cases,checks,errno,s);char p[512],buf[8192];const char*names[]={"test.runner.log","finish-restore.log","cleanup.reason","restore.phase"};for(int i=0;i<4;i++){snprintf(p,sizeof p,"%s/%s",job,names[i]);int f=open(p,O_RDONLY);if(f>=0){ssize_t n=read(f,buf,sizeof buf-1);if(n>0){buf[n]=0;printf("%s:\n%s\n",names[i],buf);}close(f);}}fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}
static void put(const char*p,const char*s){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,0600);ck(f>=0,p);ck(write(f,s,strlen(s))==(ssize_t)strlen(s),"write");close(f);}
static int get(const char*p,char*b,size_t z){int f=open(p,O_RDONLY);ssize_t n=f<0?0:read(f,b,z-1);if(f>=0)close(f);if(n<0)n=0;b[n]=0;return (int)n;}
static int has(const char*p){return !access(p,F_OK);}
static int jhas(const char*n){char p[512];snprintf(p,sizeof p,"%s/%s",job,n);return has(p);}
static int match(const char*n,const char*text){char p[512],b[4096];snprintf(p,sizeof p,"%s/%s",job,n);get(p,b,sizeof b);return !strcmp(b,text);}
static void jf(const char*n,const char*s){char p[512];snprintf(p,sizeof p,"%s/%s",job,n);put(p,s);}
static void dir(const char*p){ck(!mkdir(p,0700)||errno==EEXIST,p);}
static void cp(const char*from,const char*to){int a=open(from,O_RDONLY),b=open(to,O_WRONLY|O_CREAT|O_TRUNC,0755);ck(a>=0&&b>=0,"fixture copy");char buf[65536];ssize_t n;while((n=read(a,buf,sizeof buf))>0)ck(write(b,buf,n)==n,"fixture write");ck(n==0,"fixture read");close(a);close(b);}
static void prop(const char*n,const char*v){char p[256];snprintf(p,sizeof p,"/tmp/props/%s",n);put(p,v);}
static void clearpid(const char*path){char b[32];get(path,b,sizeof b);pid_t p=atoi(b);if(p>0)kill(p,SIGTERM);unlink(path);}
static void reset(const char*mode){cases++;printf("MEMORY_FULL_CASE %u %s\n",cases,mode);clearpid("/tmp/original.pid");clearpid("/tmp/owned.pid");usleep(1100000);while(waitpid(-1,NULL,WNOHANG)>0){}unlink("/tmp/current-owned");unlink("/tmp/exposure.count");unlink("/tmp/release-capture");rmdir("/tmp/x2d-multishot-active");put("/tmp/scenario",mode);snprintf(job,sizeof job,"/mnt/media_rw/ssd/multishot/native-album-ui-12345678-%u",cases);dir(job);jf("format-source","0\n");prop("drive_mode","1");prop("multishot_control_mode","3");prop("exposures_in_multishot_session","16");prop("image_format","2");prop("exposure_status","0");prop("storage_status","0");prop("capture_storage_mode","1");prop("stream_group","255");prop("image_bit_depth","0");prop("active_error","false");prop("eshutter_current","false");prop("live_view_state","1");}
static pid_t start_storage(void){pid_t p=fork();ck(p>=0,"storage fork");if(!p){execl("/system/bin/camera-storage","/system/bin/camera-storage",NULL);_exit(127);}for(int i=0;i<500&&!has("/tmp/original.pid");i++)usleep(10000);ck(has("/tmp/original.pid"),"original started");return p;}
static pid_t runner(const char*exe,const char*delay){pid_t p=fork();ck(p>=0,"runner fork");if(!p){char path[512];snprintf(path,sizeof path,"%s/test.runner.log",job);int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600);dup2(fd,1);dup2(fd,2);if(fd>2)close(fd);execl(exe,exe,"memory-run",job,delay,"6","--prepare",NULL);_exit(127);}return p;}
static void waitmark(const char*n,pid_t p){for(int i=0;i<1500;i++){if(jhas(n))return;int s;if(waitpid(p,&s,WNOHANG)==p){printf("early runner status=%d\n",s);ck(0,n);}usleep(10000);}ck(0,n);}
static int finish(pid_t p){for(int i=0;i<10000;i++){int s;pid_t got=waitpid(p,&s,WNOHANG);if(got==p)return WIFEXITED(s)?WEXITSTATUS(s):128+WTERMSIG(s);ck(got>=0,"wait runner");usleep(10000);}ck(0,"runner timeout");return 255;}
static void restored(int rc,int locked){char expected[32];snprintf(expected,sizeof expected,"%d\n",rc);ck(match("run.rc",expected),"final rc receipt");ck(has("/tmp/x2d-multishot-active")==locked,"global lock contract");if(!locked){ck(match("restored","RESTORED_V1\n"),"restored identity");char scenario[64];get("/tmp/scenario",scenario,sizeof scenario);ck(match("restore.snapshot",!strcmp(scenario,"live-restarted")?"MEMORY_RESTORE_SNAPSHOT_V1\n1 3 16 2 0\n":"MEMORY_RESTORE_SNAPSHOT_V1\n1 3 16 2 1\n")||match("restore.snapshot","MEMORY_RESTORE_SNAPSHOT_V1\n1 3 16 2 UNCHANGED\n"),"saved snapshot");}struct statvfs fs;ck(!statvfs("/system",&fs)&&(fs.f_flag&ST_RDONLY),"system RO");}
int main(void){setvbuf(stdout,NULL,_IONBF,0);ck(!mount("proc","/proc","proc",0,NULL),"proc");ck(!mount("devtmpfs","/dev","devtmpfs",0,NULL),"dev");ck(!mount("tmpfs","/tmp","tmpfs",0,NULL),"tmp");ck(!mount("tmpfs","/system","tmpfs",0,"size=32m"),"system");dir("/system/bin");dir("/system/lib64");dir("/system/bin/x2d-pixel-overclock");dir("/tmp/props");dir("/tmp/x2d-pixel-overclock");dir("/mnt/media_rw");dir("/mnt/media_rw/ssd");ck(!mount("tmpfs","/mnt/media_rw/ssd","tmpfs",0,"size=8g"),"synthetic capacity");dir("/mnt/media_rw/ssd/multishot");dir("/mnt/media_rw/ssd/DCIM");cp("/leaf","/bin/dbus-send");cp("/leaf","/system/bin/start");cp("/leaf","/system/bin/stop");cp("/leaf","/system/bin/camera-storage");const char*tools[]={"inputs","fix-3fr","pack"};char dst[256];for(int i=0;i<3;i++){snprintf(dst,sizeof dst,"/system/bin/x2d-pixel-overclock/%s",tools[i]);cp("/leaf",dst);}cp("/module","/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so");cp("/module","/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so.extra");put("/system/lib64/libQt6Core.so.6","inert");ck(!mount(NULL,"/system",NULL,MS_REMOUNT|MS_RDONLY,NULL),"initial RO");
    reset("exact-invalid-delay");pid_t p=runner("/exact","1");ck(finish(p)==2,"exact invalid rejected");ck(!has("/tmp/x2d-multishot-active"),"no exact lock");
    reset("exact-foreign-lock");dir("/tmp/x2d-multishot-active");p=runner("/exact","2");ck(finish(p)!=0&&has("/tmp/x2d-multishot-active"),"exact preserves foreign lock");
    reset("exact-hash-refusal");start_storage();p=runner("/exact","2");ck(finish(p)!=0&&!jhas("capture.invoked")&&!has("/tmp/x2d-multishot-active"),"exact wrong hash refuses before capture");
    #ifdef CHECKER_IMPOSTOR
    const char*scenarios[]={"mapped-impostor"};
    #else
    const char*scenarios[]={"success","live-delayed","live-restarted","cancel-prepared","cancel-countdown","cancel-file-countdown","cancel-capture","render-failure","publish-failure","missing-source-drain","restore-setting-failure"};
    #endif
    for(unsigned i=0;i<sizeof scenarios/sizeof scenarios[0];i++){
        const char*s=scenarios[i];reset(s);if(!strcmp(s,"live-restarted"))prop("live_view_state","0");start_storage();p=runner("/model","2");waitmark("prepared.ready",p);ck(!has("/tmp/owned.pid")&&!jhas("capture.invoked"),"prepare leaves original storage and exposure alone");
        if(!strcmp(s,"cancel-prepared")){kill(p,SIGTERM);ck(finish(p)==71,"prepared cancel rc");restored(71,0);ck(!jhas("capture.invoked"),"cancel no capture");continue;}
        jf("manual.trigger","GO\n");if(!strcmp(s,"cancel-countdown")){waitmark("countdown",p);kill(p,SIGTERM);ck(finish(p)==71,"countdown cancel rc");restored(71,0);ck(!jhas("capture.invoked"),"countdown no capture");continue;}
        if(!strcmp(s,"cancel-file-countdown")){waitmark("countdown",p);jf("cancel.request","CANCEL\n");ck(finish(p)==71,"cooperative countdown cancel rc");restored(71,0);ck(!jhas("capture.invoked")&&match("cancel.accepted","CANCELLED_BEFORE_CAPTURE_V1\n"),"cancel acknowledged before exposure");continue;}
        if(!strcmp(s,"cancel-capture")){waitmark("test.capture.called",p);kill(p,SIGTERM);put("/tmp/release-capture","1");}
        int expected=(!strcmp(s,"success")||!strcmp(s,"live-delayed")||!strcmp(s,"live-restarted"))?0:!strncmp(s,"cancel",6)?71:!strcmp(s,"missing-source-drain")||!strcmp(s,"restore-setting-failure")||!strcmp(s,"mapped-impostor")?90:17;int got=finish(p);printf("MEMORY_FULL_RESULT case=%u scenario=%s rc=%d\n",cases,s,got);if(!strcmp(s,"mapped-impostor"))ck(!jhas("capture.invoked"),"wrong mapped module must refuse before exposure");ck(got==expected,"full runner expected rc");restored(expected,expected==90);if(!strcmp(s,"mapped-impostor")){ck(!has("/tmp/exposure.count"),"no submitted exposure for false mapping");continue;}ck(match("test.gate.identity.saved","1\n"),"storage pid/start persisted before exec");char count[32];get("/tmp/exposure.count",count,sizeof count);ck(!strcmp(count,"1\n"),"one exposure submission");
        if(!strcmp(s,"missing-source-drain")){ck(!jhas("restored")&&!jhas("test.publish.called"),"no false restore or publish without drain");ck(has("/tmp/owned.pid"),"owned source not killed without drain");}
        if(!strcmp(s,"publish-failure"))ck(jhas("restored")&&jhas("memory.consumers.drained")&&match("cleanup.rc","0\n"),"publish primary independent from successful restore");
    }
    printf("MEMORY_FULL_ALL_PASSED cases=%u checks=%u\n",cases,checks);sync();reboot(RB_POWER_OFF);return 0;
}
