/* 4.2.0 六帧请求：先持久化 payload，再独占创建空 gate。
 * gate 出现前绝不读取正在写入的 payload；无需 exFAT renameat2。
 * 调用方仍须验证任务目录归属、固定模块摘要与 owner.ready。
 */
#ifndef X2D_MEMORY_REQUEST_GATE_H
#define X2D_MEMORY_REQUEST_GATE_H
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
static int x2d_memory_request_gate(const char *job) {
 char path[256],value[3];
 int n=snprintf(path,sizeof path,"%s/capture.requested",job);
 if(n<=0||n>=(int)sizeof path)return -1;
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0)return 0;
 ssize_t got=read(fd,value,1);close(fd);
 if(got!=0)return -1;
 n=snprintf(path,sizeof path,"%s/capture.requested.pending",job);
 if(n<=0||n>=(int)sizeof path)return -1;
 fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0)return -1;
 got=read(fd,value,sizeof value);close(fd);
 return got==2&&value[0]=='6'&&value[1]=='\n'?1:-1;
}
#endif
