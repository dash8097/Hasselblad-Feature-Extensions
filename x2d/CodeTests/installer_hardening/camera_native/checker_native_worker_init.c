#define _GNU_SOURCE
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static const char*C="/tmp/x2d-pixel-overclock";
static unsigned checks,cases,seq;
static char session[9],job[256],request[128],status[4096];
static pid_t worker;
static volatile sig_atomic_t stop;
static void term(int n){(void)n;stop=1;}
static void ck(int yes,const char*why){checks++;if(!yes){printf("WORKER_CHECK_FAIL case=%u checks=%u errno=%d %s status=%s\n",cases,checks,errno,why,status);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}
static void put(const char*p,const char*s){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,0600);ck(f>=0,p);ck(write(f,s,strlen(s))==(ssize_t)strlen(s),"write");close(f);}
static int get(const char*p,char*b,size_t z){int f=open(p,O_RDONLY|O_NONBLOCK);if(f<0){b[0]=0;return 0;}ssize_t n=read(f,b,z-1);close(f);if(n<0)n=0;b[n]=0;return (int)n;}
static void channel(const char*n,const char*s){char p[256];snprintf(p,sizeof p,"%s/%s",C,n);put(p,s);}
static void jf(const char*n,const char*s){char p[512];snprintf(p,sizeof p,"%s/%s",job,n);put(p,s);}
static int exists(const char*p){struct stat s;return !lstat(p,&s);}
static int state(const char*t){get("/tmp/x2d-pixel-overclock/status.json",status,sizeof status);return strstr(status,t)!=NULL;}
static void waitstate(const char*t){for(int n=0;n<500;n++){if(state(t))return;usleep(10000);}ck(0,t);}
static void waitfile(const char*p){for(int n=0;n<500;n++){if(exists(p))return;usleep(10000);}ck(0,p);}
static void waitjob(const char*n){char p[512];snprintf(p,sizeof p,"%s/%s",job,n);waitfile(p);}
static void dir(const char*p){ck(!mkdir(p,0700)||errno==EEXIST,p);}
static int fakeflow(int argc,char**argv){
    if(argc<5)return 120;snprintf(job,sizeof job,"%s",argv[2]);signal(SIGTERM,term);signal(SIGINT,term);
    char args[1024]={0};for(int n=1;n<argc;n++){strcat(args,argv[n]);strcat(args,"\n");}jf("test.argv",args);put("/tmp/latest-job",job);
    if(mkdir("/tmp/x2d-multishot-active",0700))return 121;
    char config[32];get("/tmp/stub-mode",config,sizeof config);jf("stage","prepare\n");
    while(!stop&&exists("/tmp/stub-hold-ready"))usleep(10000);
    if(!stop)jf("prepared.ready","PREPARED\n");
    char path[512];snprintf(path,sizeof path,"%s/manual.trigger",job);
    while(!stop&&!exists(path))usleep(10000);
    if(stop){jf("test.cancelled","cancelled\n");if(strcmp(config,"retain"))rmdir("/tmp/x2d-multishot-active");return 71;}
    char go[16];get(path,go,sizeof go);if(strcmp(go,"GO\n"))return 122;
    if(!strcmp(config,"cancel-delay")){jf("stage","delay\n");jf("test.go","1\n");snprintf(path,sizeof path,"%s/cancel.request",job);while(!stop&&!exists(path))usleep(10000);char body[32];get(path,body,sizeof body);if(strcmp(body,"CANCEL\n"))return 123;jf("cancel.accepted","CANCELLED_BEFORE_CAPTURE_V1\n");jf("run.rc","71\n");rmdir("/tmp/x2d-multishot-active");return 71;}
    jf("stage","capture\n");jf("test.go","1\n");
    snprintf(path,sizeof path,"%s/test.finish",job);while(!stop&&!exists(path))usleep(10000);
    int result=!strcmp(config,"fail")?17:0;if(!strcmp(config,"retain"))result=90;
    if(!result){jf("album-location","100HASBL/A0000001.JPG\n");jf("run.rc","0\n");}else jf("run.rc","17\n");
    if(strcmp(config,"retain"))rmdir("/tmp/x2d-multishot-active");return result;
}
static void startworker(unsigned mask){unlink("/tmp/x2d-pixel-overclock/status.json");worker=fork();ck(worker>=0,"worker fork");if(!worker){for(int n=0;n<3;n++)if(mask&(1u<<n))close(n);execl("/worker","/worker",NULL);_exit(127);}waitstate("\"stage\":\"idle\"");char*p=strstr(status,"\"session\":\"");ck(p!=NULL,"session");memcpy(session,p+11,8);session[8]=0;seq=0;}
static void stopworker(void){ck(!kill(worker,SIGTERM),"worker stop");int s;ck(waitpid(worker,&s,0)==worker&&WIFEXITED(s)&&WEXITSTATUS(s)==0,"worker orderly exit");}
static void arm(unsigned frames,unsigned delay,unsigned keep,unsigned initial,int held){
    char old[256];get("/tmp/latest-job",old,sizeof old);const char*v=frames<=2?"RS4":frames==3?"RS5":frames==6?"PO3":"PO4";
    if(frames<=3)snprintf(request,sizeof request,"%s %s %u %u %u %u %u START END\n",v,session,seq+1,frames,delay,keep,initial);
    else snprintf(request,sizeof request,"%s %s %u %u %u %u START END\n",v,session,seq+1,frames,delay,keep);
    if(held)put("/tmp/stub-hold-ready","1");channel("arm.txt",request);
    for(int n=0;n<500;n++){get("/tmp/latest-job",job,sizeof job);if(job[0]&&strcmp(job,old))break;usleep(10000);}ck(job[0]&&strcmp(job,old),"new native runner job");
    waitjob("test.argv");char args[1024],path[512],expected[1024];snprintf(path,sizeof path,"%s/test.argv",job);get(path,args,sizeof args);
    if(frames<=3)snprintf(expected,sizeof expected,"launch-stack\n%s\n%u\n%u\n%u\n%u\n--prepare\n",job,frames,delay,keep,initial);
    else snprintf(expected,sizeof expected,"%s\n%s\n%u\n%u\n--prepare\n",frames==6?"launch-memory":"launch-multi",job,delay,frames);
    ck(!strcmp(args,expected),"exact argv through worker");if(!held)waitstate("\"prepared\":true");
}
static void trigger(void){channel("request.txt",request);seq++;waitstate("\"busy\":true");waitjob("test.go");}
static void finish(int fail){jf("test.finish","1\n");waitstate(fail?"\"stage\":\"failed\"":"\"stage\":\"complete\"");waitstate("\"busy\":false");}
static void basic(unsigned f,unsigned d,unsigned k,unsigned i,unsigned format){cases++;printf("WORKER_CASE %u mode=%u format=%u\n",cases,f,format);arm(f,d,k,i,0);trigger();char b[8];snprintf(b,sizeof b,"%u\n",format);jf("format-source",b);jf("stage","render\n");waitstate("\"stage\":\"render\"");char expected[32];snprintf(expected,sizeof expected,"\"format\":%u",format);waitstate(expected);finish(0);ck(!exists("/tmp/x2d-multishot-active"),"runner owns unlock");}
int main(int argc,char**argv){if(argc>1)return fakeflow(argc,argv);setvbuf(stdout,NULL,_IONBF,0);ck(!mount("proc","/proc","proc",0,NULL),"proc");ck(!mount("devtmpfs","/dev","devtmpfs",0,NULL),"dev");ck(!mount("tmpfs","/tmp","tmpfs",0,NULL),"tmp");dir("/mnt/media_rw");dir("/mnt/media_rw/ssd");ck(!mount("tmpfs","/mnt/media_rw/ssd","tmpfs",0,"size=128m"),"synthetic SSD");startworker(0);
    for(unsigned f=0;f<3;f++)for(unsigned k=0;k<(f==2?1u:2u);k++)basic(f,10,k,2,0);
    basic(3,32,0,0,0);basic(6,2,0,0,0);basic(3,200,0,5,0);basic(16,2,0,0,1);basic(25,0,0,0,2);basic(0,30,0,0,3);
    for(unsigned format=0;format<5;format++)basic(6,2,1,0,format);
    basic(16,3,1,0,4);basic(25,0,1,0,0);basic(6,2,0,0,0);
    cases++;arm(6,2,0,0,1);channel("request.txt",request);waitstate("\"shutter_pending\":true");ck(!strstr(status,"\"busy\":true"),"pending does not capture early");unlink("/tmp/stub-hold-ready");seq++;waitstate("\"busy\":true");waitjob("test.go");finish(0);
    cases++;arm(0,30,0,0,0);trigger();jf("stack.progress","7 200 12\n");waitstate("\"stack_frames\":7");char fin[128];snprintf(fin,sizeof fin,"RSF1 %s %u FINISH END\n",session,seq);channel("finish.txt",fin);waitjob("finish.request");finish(0);
    cases++;arm(3,32,0,0,0);trigger();jf("iso.progress","100 32 4 1 125\n");waitstate("\"stack_planned_frames\":4");waitstate("\"stack_target_iso\":32");finish(0);
    cases++;arm(6,2,0,0,0);trigger();jf("stage","render\n");jf("disk400.progress","2 8 6\n");waitstate("\"jpeg_tiles\":6");waitstate("\"render_tiles\":8");finish(0);
    cases++;arm(16,2,0,0,0);trigger();jf("stage","render\n");jf("sixteen.progress","RS16 2 20 19\n");waitstate("\"jpeg_tiles\":19");waitstate("\"render_tiles\":20");finish(0);
    cases++;arm(0,10,0,0,0);char cancelled[256];snprintf(cancelled,sizeof cancelled,"%s",job);channel("arm.txt","DISARMED\n");waitstate("\"prepared\":false");char cp[512];snprintf(cp,sizeof cp,"%s/test.cancelled",cancelled);waitfile(cp);ck(!exists("/tmp/x2d-multishot-active"),"cancel restored unlock");basic(6,2,0,0,0);
    cases++;put("/tmp/stub-mode","fail");arm(16,2,0,0,0);trigger();finish(1);unlink("/tmp/stub-mode");basic(0,10,0,0,0);
    cases++;put("/tmp/stub-mode","retain");arm(6,2,0,0,0);trigger();finish(1);waitstate("\"ready\":false");ck(exists("/tmp/x2d-multishot-active"),"worker does not erase failed restore lock");unlink("/tmp/stub-mode");ck(!rmdir("/tmp/x2d-multishot-active"),"fixture restore only");basic(25,0,0,0,0);
    cases++;arm(6,2,0,0,0);trigger();char previous[256];snprintf(previous,sizeof previous,"%s",job);char other[128];snprintf(other,sizeof other,"RS4 %s %u 0 10 0 0 START END\n",session,seq+1);channel("request.txt",other);seq++;char acktext[64];snprintf(acktext,sizeof acktext,"\"ack\":%u,",seq);waitstate(acktext);finish(0);usleep(300000);get("/tmp/latest-job",job,sizeof job);ck(!strcmp(previous,job),"busy request consumed without deferred dispatch");
    cases++;snprintf(previous,sizeof previous,"%s",job);channel("request.txt","PO3 ffffffff 1 6 2 0 START END\n");usleep(300000);get("/tmp/latest-job",job,sizeof job);ck(!strcmp(previous,job)&&!state("\"busy\":true"),"stale session no dispatch");
    cases++;put("/tmp/stub-mode","retain");arm(0,10,0,0,0);snprintf(previous,sizeof previous,"%s",job);channel("arm.txt","DISARMED\n");waitstate("\"prepared\":false");waitstate("\"ready\":false");ck(exists("/tmp/x2d-multishot-active"),"cancel unconfirmed lock retained");snprintf(other,sizeof other,"PO3 %s %u 6 2 0 START END\n",session,seq+1);channel("arm.txt",other);waitstate("\"stage\":\"failed\"");get("/tmp/latest-job",job,sizeof job);ck(!strcmp(previous,job),"new mode blocked after unconfirmed cancel");unlink("/tmp/stub-mode");ck(!rmdir("/tmp/x2d-multishot-active"),"fixture resolves cancellation lock");basic(6,2,0,0,0);
    for(unsigned mode=0;mode<3;mode++){cases++;put("/tmp/stub-mode","cancel-delay");arm(mode==0?6:mode==1?16:1,10,0,10,0);trigger();waitstate("\"stage\":\"delay\"");char cancel[128],path[512];snprintf(path,sizeof path,"%s/cancel.request",job);
     snprintf(cancel,sizeof cancel,"POC1 ffffffff %u CANCEL END\n",seq);channel("finish.txt",cancel);usleep(200000);ck(!exists(path),"wrong session cancel rejected");
     snprintf(cancel,sizeof cancel,"POC1 %s %u CANCEL END\n",session,seq-1);channel("finish.txt",cancel);usleep(200000);ck(!exists(path),"stale ack cancel rejected");
     snprintf(cancel,sizeof cancel,"POC1 %s %u CANCEL END\n",session,seq);channel("finish.txt",cancel);waitstate("\"stage\":\"failed\"");waitstate("\"busy\":false");waitjob("cancel.accepted");ck(!exists("/tmp/x2d-multishot-active"),"cancelled task restored unlock");unlink("/tmp/stub-mode");}
    cases++;arm(6,2,0,0,0);trigger();char late[128],late_path[512];snprintf(late,sizeof late,"POC1 %s %u CANCEL END\n",session,seq);channel("finish.txt",late);usleep(200000);snprintf(late_path,sizeof late_path,"%s/cancel.request",job);ck(!exists(late_path),"cancel after capture start ignored");finish(0);
    stopworker();for(unsigned mask=1;mask<8;mask++){startworker(mask);basic(6,2,0,0,0);stopworker();}
    printf("NATIVE_WORKER_ALL_PASSED cases=%u checks=%u\n",cases,checks);sync();reboot(RB_POWER_OFF);return 0;
}
