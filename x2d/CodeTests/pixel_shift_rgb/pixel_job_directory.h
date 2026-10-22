/* X2D 4.2.0：在已挂载的 SSD 上补建任务父目录，不创建 SSD 挂载点。 */
#ifndef X2D_PIXEL_JOB_DIRECTORY_H
#define X2D_PIXEL_JOB_DIRECTORY_H
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef PO_STORAGE
#define PO_STORAGE "/mnt/media_rw/ssd"
#endif
#ifndef PO_MOUNTS
#define PO_MOUNTS "/proc/mounts"
#endif
static int po_make_job(const char *job,char *error,size_t capacity){
 const char prefix[]=PO_STORAGE "/multishot/";
 if(strncmp(job,prefix,sizeof prefix-1)||!job[sizeof prefix-1]){
  snprintf(error,capacity,"任务目录路径无效");return 0;
 }
 const char *leaf=job+sizeof prefix-1;
 for(const char *p=leaf;*p;p++)if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='z')||*p=='-')){
  snprintf(error,capacity,"任务目录路径无效");return 0;
 }
 FILE *mounts=fopen(PO_MOUNTS,"r");int mounted=0;
 if(mounts){
  char line[1024],source[256],target[256],type[64],options[256];
  while(fgets(line,sizeof line,mounts)){
   if(sscanf(line,"%255s %255s %63s %255s",source,target,type,options)==4&&!strcmp(target,PO_STORAGE)){
    mounted=!strncmp(options,"rw",2)&&(options[2]==','||!options[2]);break;
   }
  }
  fclose(mounts);
 }
 if(!mounted){snprintf(error,capacity,"SSD 未挂载或只读，请检查存储");return 0;}
 int disk=open(PO_STORAGE,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(disk<0){snprintf(error,capacity,"SSD 目录无法访问 (%d)",errno);return 0;}
 /* EEXIST 保留现有目录；大小写是否匹配由实际文件系统决定。 */
 if(mkdirat(disk,"multishot",0700)&&errno!=EEXIST){
  int cause=errno;close(disk);snprintf(error,capacity,"无法创建任务父目录 (%d)",cause);return 0;
 }
 int parent=openat(disk,"multishot",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);int cause=errno;
 close(disk);
 if(parent<0){snprintf(error,capacity,"任务父目录无法访问 (%d)",cause);return 0;}
 int result=mkdirat(parent,leaf,0700);cause=errno;close(parent);
 if(result){snprintf(error,capacity,"无法创建新任务目录 (%d)",cause);return 0;}
 error[0]=0;return 1;
}
#endif
