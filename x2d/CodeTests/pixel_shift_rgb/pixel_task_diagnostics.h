/* 4.2.0：任务边界日志；只保存调度信息和内存统计，不保存照片信息。 */
#ifndef PIXEL_TASK_DIAGNOSTICS_H
#define PIXEL_TASK_DIAGNOSTICS_H
static char diagnostic_job[160],diagnostic_stage[32];
static unsigned long long diagnostic_begin;
static unsigned long long diagnostic_ms(void){
 struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
 return (unsigned long long)t.tv_sec*1000+(unsigned long long)t.tv_nsec/1000000;
}
static void diagnostic_write(const char *job,const char *name,const char *text,int append){
 char path[224];int n=snprintf(path,sizeof path,"%s/%s",job,name);
 if(n<=0||n>=(int)sizeof path)return;
 int fd=open(path,O_WRONLY|O_CREAT|O_NOFOLLOW|O_NONBLOCK|(append?O_APPEND:O_TRUNC),0600);
 if(fd<0)return;
 struct stat st;
 if(!fstat(fd,&st)&&S_ISREG(st.st_mode)&&(!append||st.st_size<24576)){
  size_t left=strlen(text);const char *p=text;
  while(left){ssize_t w=write(fd,p,left);if(w<0&&errno==EINTR)continue;if(w<=0)break;p+=w;left-=(size_t)w;}
 }
 close(fd);
}
static void diagnostic_archive(const char *job,int failed){
 /* 两份有界控制记录：最近结束任务和最近失败任务。清理照片暂存后仍可诊断。 */
 /* 优先保留显影原因，避免其他长日志先耗尽6000字节摘要预算。 */
 const char *files[]={"task.info","task.result","cleanup.result","failure.result","probe.log","merge.log","task.events","task.memory",
  "phase-events","phase-metrics","stage","run.rc","capture.rc","restored","memory.failed",
  "run.log","capture.log","render.log","finish-restore.log","watchdog.log","finish-metrics"};
 char destination[224],temporary[224];
 snprintf(destination,sizeof destination,PO_CHANNEL "/%s",failed?"last-failed-task.txt":"last-task.txt");
 snprintf(temporary,sizeof temporary,"%s.tmp",destination);
 int fd=open(temporary,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_NONBLOCK,0600);if(fd<0)return;
 struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);return;}
 int any=0;size_t total=0;
 for(unsigned i=0;i<sizeof files/sizeof files[0];i++){
  char path[224],data[4097],label[80];snprintf(path,sizeof path,"%s/%s",job,files[i]);
  int source=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(source<0)continue;
  if(fstat(source,&st)||!S_ISREG(st.st_mode)){close(source);continue;}
  unsigned limit=!strcmp(files[i],"probe.log")?2048:!strcmp(files[i],"merge.log")?1024:!strcmp(files[i],"task.result")?768:256;
  if(st.st_size>(long long)limit)(void)lseek(source,st.st_size-limit,SEEK_SET);
  ssize_t n=read(source,data,limit);close(source);if(n<0)continue;
  /* 归档仅允许固定日志，禁止保存相册路径文件或 RAW 元数据。报告仍做二次脱敏。 */
  int count=snprintf(label,sizeof label,"\n[%s]%s\n",files[i],st.st_size>(long long)limit?" tail_only=1":"");
  if(total+(size_t)count+(size_t)n+1>6000)break;
  (void)write(fd,label,(size_t)count);total+=(size_t)count;any=1;
  data[n]=0;
  char *line=data;
  while(*line){char *end=strchr(line,'\n');if(end)*end=0;
   if(!strchr(line,'/')&&!strchr(line,'\\')&&!strstr(line,"serial")&&!strstr(line,"UID")&&
      !strstr(line,"HASBL")&&!strstr(line,".JPG")&&!strstr(line,".3FR")&&!strstr(line,".HEIC")){
    size_t length=strlen(line);(void)write(fd,line,length);(void)write(fd,"\n",1);total+=length+1;
   }
   if(!end)break;line=end+1;
  }
 }
 close(fd);
 if(any)(void)rename(temporary,destination);else (void)unlink(temporary);
}
static void diagnostic_memory(const char *job,const char *point){
 char memory[2048],line[256];
 if(regular_read("/proc/meminfo",memory,sizeof memory)<=0)return;
 const char *keys[]={"MemTotal:","MemFree:","MemAvailable:","CmaTotal:","CmaFree:"};
 for(unsigned i=0;i<sizeof keys/sizeof keys[0];i++){
  const char *p=strstr(memory,keys[i]);if(!p)continue;
  unsigned long value=0;if(sscanf(p+strlen(keys[i]),"%lu",&value)!=1)continue;
  snprintf(line,sizeof line,"point=%s monotonic_ms=%llu %s %lu kB\n",point,diagnostic_ms(),keys[i],value);
  diagnostic_write(job,"task.memory",line,1);
 }
}
static void diagnostic_event(const char *job,const char *event,const char *stage,int status){
 if(!job||!*job)return;
 char line[256];
 snprintf(line,sizeof line,"monotonic_ms=%llu event=%s stage=%s exit=%d signal=%d\n",
  diagnostic_ms(),event,stage,WIFEXITED(status)?WEXITSTATUS(status):-1,WIFSIGNALED(status)?WTERMSIG(status):0);
 diagnostic_write(job,"task.events",line,1);
}
static void diagnostic_start(const char *job,unsigned mode,int stack){
 snprintf(diagnostic_job,sizeof diagnostic_job,"%s",job);strcpy(diagnostic_stage,"prepare");
 diagnostic_begin=diagnostic_ms();
 const char *id=strrchr(job,'/');id=id?id+1:job;
 char line[256];snprintf(line,sizeof line,"protocol=1\njob_id=%s\nmode=%u\nstack=%d\nworker_pid=%ld\nbegin_monotonic_ms=%llu\n",id,mode,stack,(long)getpid(),diagnostic_begin);
 diagnostic_write(job,"task.info",line,0);diagnostic_event(job,"created","prepare",0);diagnostic_memory(job,"begin");
}
static void diagnostic_observe(const char *job,const char *stage){
 if(!job||!*job||!strcmp(stage,diagnostic_stage))return;
 diagnostic_event(job,"stage_end",diagnostic_stage,0);
 snprintf(diagnostic_stage,sizeof diagnostic_stage,"%s",stage);
 diagnostic_event(job,"stage_begin",stage,0);
}
static int diagnostic_marker(const char *job,const char *name){
 char path[224],data[8];snprintf(path,sizeof path,"%s/%s",job,name);
 return regular_read(path,data,sizeof data)>=0;
}
static void diagnostic_result(const char *job,const char *event,int status,int album_verified){
 if(!job||!*job)return;
 diagnostic_event(job,event,diagnostic_stage,status);diagnostic_memory(job,event);
 const char *id=strrchr(job,'/');id=id?id+1:job;
 char line[512];snprintf(line,sizeof line,
  "protocol=1\njob_id=%s\nevent=%s\nlast_observed_stage=%s\nend_monotonic_ms=%llu\nduration_ms=%llu\nexit=%d\nsignal=%d\nrun_rc_present=%d\ncapture_rc_present=%d\nrestored_marker=%d\nmemory_failed_marker=%d\njpeg_published_marker=%d\nraw_published_marker=%d\nalbum_verified=%d\n",
  id,event,diagnostic_stage,diagnostic_ms(),diagnostic_ms()-diagnostic_begin,
  WIFEXITED(status)?WEXITSTATUS(status):-1,WIFSIGNALED(status)?WTERMSIG(status):0,
  diagnostic_marker(job,"run.rc"),diagnostic_marker(job,"capture.rc"),diagnostic_marker(job,"restored"),
  diagnostic_marker(job,"memory.failed"),diagnostic_marker(job,"jpeg.published"),diagnostic_marker(job,"raw.published"),album_verified);
 /* 清理结果独立保存，不能覆盖拍摄／发布结果。 */
 const char *name=!strcmp(event,"cleanup_exit")?"cleanup.result":"task.result";
 char tmp[64];snprintf(tmp,sizeof tmp,"%s.tmp",name);diagnostic_write(job,tmp,line,0);
 char from[224],to[224];snprintf(from,sizeof from,"%s/%s",job,tmp);snprintf(to,sizeof to,"%s/%s",job,name);(void)rename(from,to);
 /* cleanup 已删掉任务时不覆盖上一份完整的归档；单独保留清理退出码。 */
 if(!strcmp(event,"cleanup_exit"))diagnostic_write(PO_CHANNEL,"last-cleanup.result",line,0);
 diagnostic_archive(job,0);
 if(!WIFEXITED(status)||WEXITSTATUS(status)!=0)diagnostic_archive(job,1);
}
#endif
