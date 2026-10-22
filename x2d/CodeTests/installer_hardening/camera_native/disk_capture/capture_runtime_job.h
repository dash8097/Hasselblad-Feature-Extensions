/* 仅接受本机手动调度器新建的八位会话号+正序号目录。 */
#ifndef CAPTURE_RUNTIME_JOB_H
#define CAPTURE_RUNTIME_JOB_H
#include <stddef.h>
#include <string.h>
static inline int capture_runtime_job(char out[108],const char *path){
 const char prefix[]="/mnt/media_rw/ssd/multishot/native-album-ui-";
 if(!out||!path||strncmp(path,prefix,sizeof prefix-1))return 0;
 size_t n=sizeof prefix-1;
 for(unsigned i=0;i<8;i++,n++)if(!((path[n]>='0'&&path[n]<='9')||(path[n]>='a'&&path[n]<='f')))return 0;
 if(path[n++]!='-'||path[n]<'1'||path[n]>'9')return 0;
 size_t first=n;
 while(path[n]>='0'&&path[n]<='9'&&n-first<10)n++;
 if(path[n]||n>=96)return 0;
 memcpy(out,path,n+1);return 1;
}
#endif
