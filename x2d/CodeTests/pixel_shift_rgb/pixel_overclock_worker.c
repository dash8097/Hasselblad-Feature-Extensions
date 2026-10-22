/* X2D 4.2.0 本机手动任务调度；仅为用户已按下的快门保留一次待拍请求。 */
#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "pixel_overclock_core.h"
#include "pixel_mode_events.h"
#include "pixel_idle_wait.h"
#include "pixel_shutter_intent.h"
#include "pixel_job_directory.h"
#include "sixteen_progress.h"
#ifndef PO_CHANNEL
#define PO_CHANNEL "/tmp/x2d-pixel-overclock"
#endif
#ifndef PO_JOBS
#define PO_JOBS "/mnt/media_rw/ssd/multishot"
#endif
#ifndef PO_RUNNER
#define PO_RUNNER "/system/etc/x2d-pixel-overclock/launch.sh"
#endif
#ifndef PO_SHELL
#define PO_SHELL "/system/bin/sh"
#endif
#ifndef PO_CLEANER
#define PO_CLEANER "/system/etc/x2d-pixel-overclock/cleanup-job.sh"
#endif
#ifndef PO_MODE_PREPARE
#define PO_MODE_PREPARE "/system/etc/x2d-pixel-overclock/mode-prepare.sh"
#endif
#ifndef PO_FULL_PREPARE
#define PO_FULL_PREPARE 0
#endif
static volatile sig_atomic_t stopping;
static unsigned selected_frames=6,selected_delay=2,selected_keep=0,selected_format=0,remaining=0;
static unsigned work_done=0,work_total=17,render_tiles=0,jpeg_tiles=0;
static unsigned jpeg_join_rows=0,jpeg_join_total=0;
static int jpeg_join_complete=0;
static unsigned mode_epoch=0;
static int mode_preparing=0,mode_prepared=0;
static unsigned wait_token=0;
static int mode_pending=0;
static char prepared_arm[128];
#ifdef PO_RAW_STACK
static int stack_selected;
static unsigned stack_frames,stack_gap_ms,stack_remaining;
static unsigned stack_source_iso,stack_target_iso,stack_planned_frames,stack_shutter_n,stack_shutter_d;
#endif
static void stop_now(int n){(void)n;stopping=1;}
static int disarm(void){
 int fd=open(PO_CHANNEL "/arm.worker.tmp",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
 if(fd<0)return 0;int ok=write(fd,"DISARMED\n",9)==9;close(fd);
 return ok&&!rename(PO_CHANNEL "/arm.worker.tmp",PO_CHANNEL "/arm.txt");
}
static int regular_read(const char *path,char *data,size_t size){
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return -1;
 struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);return -1;}
 ssize_t n=read(fd,data,size-1);close(fd);if(n<0)return -1;data[n]=0;return (int)n;
}
#include "pixel_task_diagnostics.h"
#ifdef PO_RAW_STACK
#define DIAGNOSTIC_STACK(mode) ((mode)<=3)
#else
#define DIAGNOSTIC_STACK(mode) 0
#endif
/* 只回收本worker刚等待结束的任务；标记在缓存释放、原厂恢复及取景恢复后才写入。 */
static void release_finished_lock(const char *job){
 if(!job||!*job)return;
 char path[192],value[32];
 snprintf(path,sizeof path,"%s/lock.releasable",job);
 if(regular_read(path,value,sizeof value)!=0)return;
 snprintf(path,sizeof path,"%s/restored",job);
 if(regular_read(path,value,sizeof value)!=0)return;
 snprintf(path,sizeof path,"%s/run.rc",job);
 if(regular_read(path,value,sizeof value)<=0)return;
 char *end;long rc=strtol(value,&end,10);
 if(rc<0||rc>255||strcmp(end,"\n"))return;
 struct stat st;
 if(!lstat("/tmp/x2d-multishot-active",&st)&&S_ISDIR(st.st_mode)&&st.st_uid==getuid())
  (void)rmdir("/tmp/x2d-multishot-active");
}
static int status_file(const char *session,unsigned ack,int busy,int ready,unsigned heartbeat,
                       const char *stage,const char *album,const char *error){
 char line[1536];int n=snprintf(line,sizeof line,
  "{\"protocol\":3,\"session\":\"%s\",\"ack\":%u,\"busy\":%s,\"ready\":%s,\"heartbeat\":%u,\"stage\":\"%s\",\"album\":\"%s\",\"error\":\"%s\",\"frames\":%u,\"delay\":%u,\"keep\":%u,\"format\":%u,\"remaining\":%u,\"work_done\":%u,\"work_total\":%u,\"render_tiles\":%u,\"mode_epoch\":%u,\"preparing\":%s,\"prepared\":%s}\n",
  session,ack,busy?"true":"false",ready?"true":"false",heartbeat,stage,album,error,selected_frames,selected_delay,selected_keep,selected_format,remaining,work_done,work_total,render_tiles,mode_epoch,mode_preparing?"true":"false",mode_prepared?"true":"false");
 if(n>2&&n<(int)sizeof line){n-=2;n+=snprintf(line+n,sizeof line-(size_t)n,
  ",\"wait_token\":%u,\"shutter_pending\":%s,\"prepared_arm\":\"%s\",\"jpeg_tiles\":%u,\"jpeg_join_rows\":%u,\"jpeg_join_total\":%u,\"jpeg_join_complete\":%s}\n",wait_token,mode_pending?"true":"false",prepared_arm,jpeg_tiles,jpeg_join_rows,jpeg_join_total,jpeg_join_complete?"true":"false");}
#ifdef PO_RAW_STACK
 if(n>2&&n<(int)sizeof line){n-=2;n+=snprintf(line+n,sizeof line-(size_t)n,
  ",\"stack\":%s,\"stack_frames\":%u,\"stack_gap_ms\":%u,\"stack_remaining\":%u,\"stack_iso_mode\":%s,\"stack_source_iso\":%u,\"stack_target_iso\":%u,\"stack_planned_frames\":%u,\"stack_shutter_n\":%u,\"stack_shutter_d\":%u}\n",
  stack_selected?"true":"false",stack_frames,stack_gap_ms,stack_remaining,
  stack_selected&&selected_frames==3?"true":"false",stack_source_iso,stack_target_iso,stack_planned_frames,stack_shutter_n,stack_shutter_d);}
#endif
 if(n<=0||n>=(int)sizeof line)return 0;
 int fd=open(PO_CHANNEL "/status.tmp",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
 if(fd<0)return 0;int ok=write(fd,line,(size_t)n)==n;close(fd);
 return ok&&!rename(PO_CHANNEL "/status.tmp",PO_CHANNEL "/status.json");
}
static int stage_valid(const char *s){
 const char *names[]={"prepare","delay","capture","merge","render","jpeg","jpeg_join","package","convert","raw","publish","restore"};
 for(unsigned i=0;i<sizeof names/sizeof names[0];i++)if(!strcmp(s,names[i]))return 1;return 0;
}
static int album_valid(const char *p){
#ifdef PO_RAW_STACK
 if(stack_selected){
  size_t n=strlen(p);if((n!=22&&n!=23)||memcmp(p+3,"HASBL/",6))return 0;
  for(unsigned i=0;i<3;i++)if(p[i]<'0'||p[i]>'9'||(!i&&p[i]=='0'))return 0;
  if(p[9]<'A'||p[9]>'Z')return 0;
  for(unsigned i=10;i<17;i++)if(p[i]<'0'||p[i]>'9')return 0;
  return !strcmp(p+17,".JPG\n")||!strcmp(p+17,".HEIC\n");
 }
#endif
 if(strlen(p)!=22||p[0]<'1'||p[0]>'9'||p[1]<'0'||p[1]>'9'||p[2]<'0'||p[2]>'9'||memcmp(p+3,"HASBL/",6)||p[9]<'A'||p[9]>'Z'||memcmp(p+17,".JPG\n",5))return 0;
 for(unsigned i=10;i<17;i++)if(p[i]<'0'||p[i]>'9')return 0;
 return 1;
}
static void prepare_stop(pid_t *process){
 if(!*process)return;
 /* 接管存储的 runner 必须自行恢复，不能同时杀掉它的存储子进程。 */
 if(!PO_FULL_PREPARE)(void)kill(-*process,SIGTERM);
 (void)kill(*process,SIGTERM);
 int status=0;pid_t ended;
 do{ended=waitpid(*process,&status,0);}while(ended<0&&errno==EINTR);
 if(ended==*process)diagnostic_result(diagnostic_job,"prepare_cancelled",status,0);
 *process=0;
}
#if !PO_FULL_PREPARE
static pid_t prepare_start(int lock,int notifications,const char *operation){
 pid_t process=fork();
 if(process==0){
  close(lock);close(notifications);(void)setpgid(0,0);
  int fd=open("/dev/null",O_WRONLY);if(fd>=0){dup2(fd,1);dup2(fd,2);close(fd);}
  execl(PO_SHELL,"sh",PO_MODE_PREPARE,operation,(char *)0);_exit(103);
 }
 if(process<0)return 0;
 (void)setpgid(process,process);return process;
}
#else
static pid_t prepared_start(int lock,int notifications,const char *job,const PoRequest *request){
 pid_t p=fork();
 if(p==0){
  close(lock);close(notifications);(void)setpgid(0,0);
  char log[192],delay[12],frames[12];snprintf(log,sizeof log,"%s/run.log",job);
  snprintf(delay,sizeof delay,"%u",request->delay);snprintf(frames,sizeof frames,"%u",request->frames);
  int fd=open(log,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)_exit(100);
  dup2(fd,1);dup2(fd,2);close(fd);
#ifdef PO_RAW_STACK
  if(request->frames<=3){char noise[2]={(char)('0'+request->keep),0},initial[12];
   snprintf(initial,sizeof initial,"%u",request->initial_delay);
   execl(PO_SHELL,"sh","/system/etc/x2d-raw-stack/launch.sh",job,frames,delay,noise,initial,"--prepare",(char*)0);_exit(101);}
#endif
#ifdef PO_MULTI_RESOLUTION
  if(request->frames==16||request->frames==25){
   execl(PO_SHELL,"sh","/system/etc/x2d-multi-resolution/launch.sh",job,delay,frames,"--prepare",(char*)0);_exit(101);
  }
#endif
  execl(PO_SHELL,"sh",PO_RUNNER,job,delay,frames,"--prepare",(char*)0);_exit(101);
 }
 if(p<0)return 0;(void)setpgid(p,p);return p;
}
static int prepared_ready(const char *job){
 char path[192],value[32];snprintf(path,sizeof path,"%s/prepared.ready",job);
 return regular_read(path,value,sizeof value)==9&&!strcmp(value,"PREPARED\n");
}
static int prepared_trigger(const char *job){
 char path[192],tmp[192];snprintf(path,sizeof path,"%s/manual.trigger",job);
 snprintf(tmp,sizeof tmp,"%s/manual.trigger.tmp",job);
 struct stat st;if(!lstat(path,&st)||errno!=ENOENT)return 0;
 int fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return 0;
 int ok=write(fd,"GO\n",3)==3;close(fd);
 return ok&&!rename(tmp,path);
}
#endif
int main(void){
 /* 由 camera-service 启动时脱离其进程组，受控显影重启服务不会结束本次任务。 */
 (void)setsid();
 umask(0077);if(mkdir(PO_CHANNEL,0700)&&errno!=EEXIST)return 2;
 struct stat st;if(lstat(PO_CHANNEL,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=getuid())return 2;
 int lock=open(PO_CHANNEL "/worker.lock",O_RDWR|O_CREAT|O_NOFOLLOW,0600);
 if(lock<0||flock(lock,LOCK_EX|LOCK_NB))return 3;
 if(!disarm())return 6;
 int random=open("/dev/urandom",O_RDONLY);unsigned seed=0;
 if(random<0||read(random,&seed,sizeof seed)!=sizeof seed)return 4;close(random);
 char session[9];snprintf(session,sizeof session,"%08x",seed);
 unsigned ack=0,beat=0;pid_t child=0;int cleaning=0;char job[160]={0},stage[32]="idle",album[64]={0},error[64]={0};
 signal(SIGTERM,stop_now);signal(SIGINT,stop_now);
 int notifications=inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
 if(notifications<0||inotify_add_watch(notifications,PO_CHANNEL,IN_CLOSE_WRITE|IN_MOVED_TO|IN_DELETE)<0)return 7;
 pid_t preparing=0;char previous_arm[129]={0};int pending_arm=0;
#if PO_FULL_PREPARE
 char prepared_job[160]={0};unsigned job_counter=0;
#endif
 while(!stopping){
  int prepare_rc=0;if(preparing&&waitpid(preparing,&prepare_rc,WNOHANG)==preparing){
   diagnostic_result(diagnostic_job,"prepare_exit",prepare_rc,0);
   preparing=0;mode_pending=0;previous_arm[0]=0;
#if PO_FULL_PREPARE
   disarm();strcpy(stage,"failed");strcpy(error,"模式准备失败，请查看任务日志");
#endif
  }
  char events[4096];ssize_t event_bytes=read(notifications,events,sizeof events);int arm_changed=0;
  if(event_bytes>0&&!po_arm_event(events,(size_t)event_bytes,&arm_changed)){stopping=1;break;}
  if(event_bytes<0&&errno!=EAGAIN&&errno!=EINTR){stopping=1;break;}
  arm_changed|=pending_arm;pending_arm=0;
  if(arm_changed&&!child){
   mode_pending=0;
   char arm[129];int arm_bytes=regular_read(PO_CHANNEL "/arm.txt",arm,sizeof arm);PoRequest applied;
#if PO_FULL_PREPARE
   if(arm_bytes>0&&po_parse_request(arm,(size_t)arm_bytes,session,ack,&applied)){
    if(strcmp(arm,previous_arm)){
     if(mode_epoch>=2147483646u){stopping=1;break;}
     mode_epoch++;
     prepare_stop(&preparing);release_finished_lock(prepared_job);strcpy(previous_arm,arm);
     int len=snprintf(prepared_job,sizeof prepared_job,PO_JOBS "/native-album-ui-%s-%u",session,++job_counter);
     int directory_ok=0;
     if(access("/tmp/x2d-multishot-active",F_OK)==0)strcpy(error,"模式恢复未完成，请检查相机");
     else if(len<=0||len>=(int)sizeof prepared_job)strcpy(error,"任务目录路径无效");
     else directory_ok=po_make_job(prepared_job,error,sizeof error);
     if(!directory_ok){
      previous_arm[0]=0;disarm();strcpy(stage,"failed");
     }else{
      selected_delay=applied.delay;selected_frames=applied.frames;selected_keep=applied.keep;
#ifdef PO_RAW_STACK
      stack_selected=applied.frames<=3;stack_frames=stack_gap_ms=0;stack_remaining=stack_selected?applied.delay:0;
      stack_source_iso=stack_planned_frames=stack_shutter_n=stack_shutter_d=0;
      stack_target_iso=applied.frames==3?applied.delay:0;
      if(stack_selected)selected_keep=0;
#endif
      strcpy(stage,"prepare");error[0]=album[0]=0;
      diagnostic_start(prepared_job,applied.frames,DIAGNOSTIC_STACK(applied.frames));
      preparing=prepared_start(lock,notifications,prepared_job,&applied);
      if(!preparing){previous_arm[0]=0;disarm();strcpy(stage,"failed");strcpy(error,"无法启动模式准备");}
     }
    }
   }else{
    prepare_stop(&preparing);release_finished_lock(prepared_job);previous_arm[0]=0;
    if(!strcmp(stage,"prepare"))strcpy(stage,"idle");
   }
#else
   if(arm_bytes>0&&po_parse_request(arm,(size_t)arm_bytes,session,ack,&applied)&&strcmp(arm,previous_arm)&&
      access("/tmp/x2d-multishot-active",F_OK)!=0){
    prepare_stop(&preparing);strcpy(previous_arm,arm);preparing=prepare_start(lock,notifications,"--prepare");
   }else if(arm_bytes<=0||!po_parse_request(arm,(size_t)arm_bytes,session,ack,&applied)){
    prepare_stop(&preparing);previous_arm[0]=0;
    preparing=prepare_start(lock,notifications,"--clear");
   }
#endif
  }
  char request[129]={0},request_record[256];
  int n=regular_read(PO_CHANNEL "/request.txt",request_record,sizeof request_record);PoRequest r;
  if(n>0&&!memcmp(request_record,"POQ1 ",5)){
   struct stat arm_stat;uint64_t generation[3]={0};
   if(!lstat(PO_CHANNEL "/arm.txt",&arm_stat)&&S_ISREG(arm_stat.st_mode)){
    generation[0]=(uint64_t)arm_stat.st_ino;generation[1]=(uint64_t)arm_stat.st_mtim.tv_sec;generation[2]=(uint64_t)arm_stat.st_mtim.tv_nsec;
    n=po_intent_read(request_record,(size_t)n,generation,request);
   }else n=0;
   if(!n){(void)unlink(PO_CHANNEL "/request.txt");mode_pending=0;}
  }else if(n>0&&n<(int)sizeof request){memcpy(request,request_record,(size_t)n+1);}
  else{n=0;mode_pending=0;}

  if(n>0&&po_parse_request(request,(size_t)n,session,ack,&r)){
   if(child){ack=r.sequence;/* 忙时消费编号并拒绝；绝不在结束后补拍。 */}
#if PO_FULL_PREPARE
   else if(preparing&&!strcmp(request,previous_arm)&&!prepared_ready(prepared_job)){
    /* 用户已经按下快门：保留同一份意图和准备任务，就绪后兑现一次。 */
    if(!mode_pending)wait_token++;
    mode_pending=1;strcpy(stage,"prepare");error[0]=0;
   }
   else if(preparing&&!strcmp(request,previous_arm)&&prepared_ready(prepared_job)&&prepared_trigger(prepared_job)){
    ack=r.sequence;mode_pending=0;(void)unlink(PO_CHANNEL "/request.txt");
    child=preparing;preparing=0;strcpy(job,prepared_job);
    diagnostic_event(job,"manual_shutter_accepted","prepare",0);
    selected_frames=r.frames;selected_delay=r.delay;selected_keep=r.keep;selected_format=0;
#ifdef PO_RAW_STACK
    if(stack_selected)selected_keep=0;
#endif
    remaining=r.delay;work_done=render_tiles=0;strcpy(stage,"delay");album[0]=error[0]=0;
#ifdef PO_RAW_STACK
    if(stack_selected){remaining=r.initial_delay;if(!remaining)strcpy(stage,"capture");}
#endif
   }else{
    ack=r.sequence;
    mode_pending=0;(void)unlink(PO_CHANNEL "/request.txt");
    disarm();prepare_stop(&preparing);previous_arm[0]=0;strcpy(stage,"failed");strcpy(error,"模式尚未准备就绪，请重新应用");
   }
#else
   else if(access("/tmp/x2d-multishot-active",F_OK)==0){ack=r.sequence;disarm();strcpy(stage,"failed");strcpy(error,"恢复未确认，请检查相机");}
   else{
    ack=r.sequence;
    prepare_stop(&preparing);
    int len=snprintf(job,sizeof job,PO_JOBS "/native-album-ui-%s-%u",session,ack);
    int directory_ok=0;
    if(len<=0||len>=(int)sizeof job)strcpy(error,"任务目录路径无效");
    else directory_ok=po_make_job(job,error,sizeof error);
    if(!directory_ok){
     disarm();strcpy(stage,"failed");
    }else{
     selected_frames=r.frames;selected_delay=r.delay;selected_keep=r.keep;selected_format=0;
     remaining=0;work_done=0;render_tiles=0;
     strcpy(stage,"prepare");album[0]=error[0]=0;
     diagnostic_start(job,r.frames,DIAGNOSTIC_STACK(r.frames));
     child=fork();
     if(child==0){
      close(lock);char log[192],delay[12],frames[12];snprintf(log,sizeof log,"%s/run.log",job);
      snprintf(delay,sizeof delay,"%u",r.delay);
      snprintf(frames,sizeof frames,"%u",r.frames);
      int fd=open(log,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)_exit(100);
      dup2(fd,1);dup2(fd,2);close(fd);
      execl(PO_SHELL,"sh",PO_RUNNER,job,delay,frames,(char *)0);_exit(101);
     }
     if(child<0){child=0;disarm();strcpy(stage,"failed");strcpy(error,"无法启动任务");}
    }
   }
#endif
  }
  if(child){
   char path[192],value[64];snprintf(path,sizeof path,"%s/stage",job);
   if(!cleaning){
    char cancel[128],expected[128];snprintf(expected,sizeof expected,"POC1 %s %u CANCEL END\n",session,ack);
    // Reuse the fixed control endpoint, binding the request to this session
    // and capture acknowledgement. The runner alone accepts it before exposure.
    if(regular_read(PO_CHANNEL "/finish.txt",cancel,sizeof cancel)>0&&!strcmp(cancel,expected)&&
       regular_read(path,value,sizeof value)>0&&!strcmp(value,"delay\n")){
     char cancel_path[192];snprintf(cancel_path,sizeof cancel_path,"%s/cancel.request",job);
     int fd=open(cancel_path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
     if(fd>=0){if(write(fd,"CANCEL\n",7)!=7)stopping=1;close(fd);}
    }
   }
#ifdef PO_RAW_STACK
   if(stack_selected&&!cleaning){
    char finish[128],expected[128];snprintf(expected,sizeof expected,"RSF1 %s %u FINISH END\n",session,ack);
    if(regular_read(PO_CHANNEL "/finish.txt",finish,sizeof finish)>0&&!strcmp(finish,expected)){
     snprintf(path,sizeof path,"%s/finish.request",job);
     int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
     if(fd>=0){if(write(fd,"FINISH\n",7)!=7)stopping=1;close(fd);}
    }
    snprintf(path,sizeof path,"%s/stack.progress",job);
    if(regular_read(path,value,sizeof value)>0){unsigned f,g,t;char end;
     if(sscanf(value,"%u %u %u%c",&f,&g,&t,&end)==4&&end=='\n'&&f<=2147483646&&t<=86399){stack_frames=f;stack_gap_ms=g;stack_remaining=t;}}
    if(selected_frames==3){
     snprintf(path,sizeof path,"%s/iso.progress",job);
     if(regular_read(path,value,sizeof value)>0){unsigned source,target,planned,numerator,denominator;int used=0;
      if(sscanf(value,"%u %u %u %u %u\n%n",&source,&target,&planned,&numerator,&denominator,&used)==5&&
         !value[used]&&source&&source<=65535&&target&&target==selected_delay&&target<=source&&
         planned==(source+target-1)/target&&numerator&&denominator){
       stack_source_iso=source;stack_target_iso=target;stack_planned_frames=planned;
       stack_shutter_n=numerator;stack_shutter_d=denominator;
      }
     }
    }
    snprintf(path,sizeof path,"%s/stage",job);
   }
#endif
   if(!cleaning){
   n=regular_read(path,value,sizeof value);
   if(n>0){value[strcspn(value,"\r\n")]=0;if(stage_valid(value))strcpy(stage,value);}
   remaining=0;
   if(!strcmp(stage,"delay")){
    snprintf(path,sizeof path,"%s/countdown",job);n=regular_read(path,value,sizeof value);
    if(n>0){char *end;unsigned long v=strtoul(value,&end,10);if(v<=60&&end!=value&&(!strcmp(end,"\n")||!*end))remaining=(unsigned)v;}
   }
   snprintf(path,sizeof path,"%s/format-source",job);n=regular_read(path,value,sizeof value);
   if(n==2&&value[0]>='0'&&value[0]<='4'&&value[1]=='\n')selected_format=(unsigned)(value[0]-'0');
   unsigned raw=selected_format==0||selected_format==1||selected_format==3;
#ifdef PO_RAW_STACK
   if(stack_selected)raw=1;
#endif
   unsigned heif=selected_format==3||selected_format==4;
   jpeg_tiles=0;jpeg_join_rows=jpeg_join_total=0;jpeg_join_complete=0;
   work_total=16+raw+heif;
   if(!strcmp(stage,"merge"))work_done=1;
   else if(!strcmp(stage,"render")){
    work_done=2;
    char log[8192];snprintf(path,sizeof path,"%s/probe.log",job);
    if(regular_read(path,log,sizeof log)>0){
     render_tiles=0;for(unsigned i=0;i<9;i++){char marker[32];snprintf(marker,sizeof marker,"TILE_COPIED %u\n",i);if(strstr(log,marker))render_tiles++;}
     unsigned jpeg=0;const char *p=log;while((p=strstr(p,"NATIVE_JPEG_SAVED bytes="))!=NULL){jpeg++;p++;}
     if(jpeg>2)jpeg=2;
     work_done=2+render_tiles+jpeg;
    }
   }else if(!strcmp(stage,"raw"))work_done=13;
   else if(!strcmp(stage,"convert"))work_done=13+raw;
   else if(!strcmp(stage,"publish"))work_done=13+raw+heif;
   else if(!strcmp(stage,"restore"))work_done=14+raw+heif;
#ifdef PO_NATIVE_DISK400
   if(selected_frames==6){
    if(!strcmp(stage,"render")||!strcmp(stage,"jpeg")){
     unsigned phase,done,encoded;int used=0;
     snprintf(path,sizeof path,"%s/disk400.progress",job);
     if(regular_read(path,value,sizeof value)>0&&sscanf(value,"%u %u %u %n",&phase,&done,&encoded,&used)==3&&
        value[used]==0&&phase>=1&&phase<=3&&done<=9&&encoded<=done){
      render_tiles=done;jpeg_tiles=encoded;work_done=2+(done+encoded)*10/18;
      strcpy(stage,phase==1?"render":"jpeg");
     }
    }else if(!strcmp(stage,"jpeg_join")){
     render_tiles=jpeg_tiles=9;work_done=12;
     char join_log[8192];snprintf(path,sizeof path,"%s/jpeg-join.log",job);
     if(regular_read(path,join_log,sizeof join_log)>0){
      const char *at=join_log;unsigned rows,total;
      while((at=strstr(at,"JPEG_GRID_COMPRESSED_ROWS "))){
       if(sscanf(at,"JPEG_GRID_COMPRESSED_ROWS %u/%u",&rows,&total)==2&&total==17498&&rows<=total){
        jpeg_join_rows=rows;jpeg_join_total=total;jpeg_join_complete=rows==total;
       }
       at++;
      }
     }
    }
   }
#endif
#ifdef PO_RAW_STACK
   if(stack_selected){
    /* 单幅堆栈显影不借用九块位移组的进度口径。 */
    work_total=5+heif;
    if(!strcmp(stage,"capture")||!strcmp(stage,"delay"))work_done=0;
    else if(!strcmp(stage,"merge"))work_done=0;
    else if(!strcmp(stage,"restore"))work_done=1;
    else if(!strcmp(stage,"render"))work_done=2;
    else if(!strcmp(stage,"raw"))work_done=3;
    else if(!strcmp(stage,"convert"))work_done=4;
    else if(!strcmp(stage,"publish"))work_done=4+heif;
   }
#endif
#ifdef PO_MULTI_RESOLUTION
   if(selected_frames==25){
    work_total=5;
    if(!strcmp(stage,"capture")||!strcmp(stage,"delay"))work_done=0;
    else if(!strcmp(stage,"merge")||!strcmp(stage,"restore"))work_done=1;
    else if(!strcmp(stage,"render"))work_done=2;
    else if(!strcmp(stage,"raw"))work_done=3;
    else if(!strcmp(stage,"publish"))work_done=4;
    render_tiles=0;
   }
   if(selected_frames==16){
    work_total=55;render_tiles=0;work_done=0;
    if(!strcmp(stage,"merge")||!strcmp(stage,"restore"))work_done=1;
    else if(!strcmp(stage,"render")||!strcmp(stage,"jpeg")){
     work_done=2;unsigned progress_phase;
     snprintf(path,sizeof path,"%s/sixteen.progress",job);
     if(regular_read(path,value,sizeof value)>0&&sixteen_progress_parse(value,&progress_phase,&render_tiles,&jpeg_tiles)){
      strcpy(stage,progress_phase==1?"render":"jpeg");work_done=2+render_tiles+jpeg_tiles;
     }
    }else if(!strcmp(stage,"jpeg_join")){
     render_tiles=jpeg_tiles=25;work_done=52;
     char join_log[8192];snprintf(path,sizeof path,"%s/jpeg-join.log",job);
     if(regular_read(path,join_log,sizeof join_log)>0)
      (void)sixteen_join_progress_parse(join_log,&jpeg_join_rows,&jpeg_join_total,&jpeg_join_complete);
    }
    else if(!strcmp(stage,"package")){render_tiles=jpeg_tiles=25;work_done=53;}
    else if(!strcmp(stage,"publish")||!strcmp(stage,"raw")){render_tiles=jpeg_tiles=25;work_done=54;}
   }
#endif
   }
   diagnostic_observe(job,stage);
   int rc=0;pid_t done=waitpid(child,&rc,WNOHANG);
   if(done==child){
    diagnostic_result(job,cleaning?"cleanup_exit":"runner_exit",rc,0);
    child=0;
    remaining=0;
    if(cleaning){
     cleaning=0;strcpy(stage,"complete");work_done=work_total;
     if(!WIFEXITED(rc)||WEXITSTATUS(rc)!=0){strcpy(error,"成片已保存，中间文件未清理");work_done=work_total-1;}
    }else if(WIFEXITED(rc)&&WEXITSTATUS(rc)==0){
     snprintf(path,sizeof path,"%s/album-location",job);n=regular_read(path,value,sizeof value);
     /* 只显示自有目录/图号，无原始照片路径。 */
     if(n>0&&album_valid(value)){
      diagnostic_result(job,"runner_exit",rc,1);
      value[strcspn(value,"\r\n")]=0;strcpy(album,value);strcpy(stage,"cleanup");work_done=work_total-1;
#ifdef PO_RAW_STACK
      if(stack_selected){strcpy(stage,"complete");work_done=work_total;disarm();continue;}
#endif
#ifdef PO_MULTI_RESOLUTION
      /* 新模式的runner仅在发布后清理自有暂存文件，不能调用六帧清理器。 */
      if(selected_frames==16||selected_frames==25){strcpy(stage,"complete");work_done=work_total;if(!selected_keep)disarm();continue;}
#endif
#ifdef PO_NATIVE_DISK400
      if(selected_frames==6){strcpy(stage,"complete");work_done=work_total;if(!selected_keep)disarm();continue;}
#endif
      child=fork();
      if(child==0){
       close(lock);char format[2]={(char)('0'+selected_format),0};
       int fd=open("/dev/null",O_WRONLY);if(fd>=0){dup2(fd,1);dup2(fd,2);close(fd);}
       execl(PO_SHELL,"sh",PO_CLEANER,job,album,format,(char *)0);_exit(102);
      }
      if(child>0)cleaning=1;
      else{child=0;strcpy(stage,"complete");strcpy(error,"成片已保存，中间文件未清理");}
     }else{diagnostic_result(job,"album_verify_failed",1<<8,0);strcpy(stage,"failed");strcpy(error,"相册核验未完成");}
    }else{strcpy(stage,"failed");snprintf(path,sizeof path,"%s/cancel.accepted",job);
     if(WIFEXITED(rc)&&WEXITSTATUS(rc)==71&&regular_read(path,value,sizeof value)>0&&!strcmp(value,"CANCELLED_BEFORE_CAPTURE_V1\n"))strcpy(error,"拍摄已取消");
     else strcpy(error,"处理失败，请查看任务日志");}
    if(!child&&(!selected_keep||strcmp(stage,"complete")))disarm();
   }
  }
  int ready=access(PO_RUNNER,R_OK)==0&&access("/tmp/x2d-multishot-active",F_OK)!=0;
  mode_preparing=mode_prepared=0;
  prepared_arm[0]=0;
#if PO_FULL_PREPARE
  if(preparing){ready=prepared_ready(prepared_job);mode_prepared=ready;mode_preparing=!ready;if(ready){
   strcpy(stage,"idle");strcpy(prepared_arm,previous_arm);prepared_arm[strcspn(prepared_arm,"\n")]=0;
  }diagnostic_observe(prepared_job,mode_prepared?"prepared":"prepare");}
#endif
  if(!status_file(session,ack,child!=0,ready||child!=0,++beat,stage,album,error)){stopping=1;break;}
  if(!po_idle_wait(notifications,(child||preparing||mode_pending)?250:1000,&pending_arm)){stopping=1;break;}
 }
 /* 正在处理时保持运行直至完整恢复，防止调度退出留下仍可重叠的新会话。 */
 prepare_stop(&preparing);close(notifications);
 if(child){while(waitpid(child,0,0)<0&&errno==EINTR){}}
 disarm();status_file(session,ack,0,0,++beat,"offline","","");close(lock);return 0;
}
