#ifndef PIXEL_IDLE_WAIT_H
#define PIXEL_IDLE_WAIT_H
#include <poll.h>
/* 目录自身的状态输出不得唤醒调度；只接受用户命令和准备回执。
   使用单调期限，连续的无关文件事件不会把下一次心跳无限推迟。 */
static int po_command_event(const void *buffer,size_t bytes,int *arm,int *wake){
 const unsigned char *p=buffer;size_t at=0;
 *arm=0;*wake=0;
 while(at<bytes){
  uint32_t h[4];if(bytes-at<sizeof h)return 0;memcpy(h,p+at,sizeof h);
  if(h[3]>bytes-at-sizeof h)return 0;
  if(h[1]&0xc000u){*arm=1;*wake=1;} /* overflow / ignored */
  if(h[3]){
   const char *name=(const char*)p+at+sizeof h;if(!memchr(name,0,h[3]))return 0;
   if(!strcmp(name,"arm.txt")){*arm=1;*wake=1;}
   if(!strcmp(name,"request.txt")||!strcmp(name,"finish.txt")||!strcmp(name,"prepared.txt"))*wake=1;
  }
  at+=sizeof h+h[3];
 }
 return 1;
}
static long long po_clock_ms(void){
 struct timespec ts;if(clock_gettime(CLOCK_MONOTONIC,&ts))return -1;
 return (long long)ts.tv_sec*1000+ts.tv_nsec/1000000;
}
static int po_idle_wait(int fd,int timeout,int *pending_arm){
 long long start=po_clock_ms();if(start<0)return 0;
 for(;;){
  long long now=po_clock_ms();if(now<0)return 0;
  long long left=start+timeout-now;if(left<=0)return 1;
  struct pollfd p={fd,POLLIN,0};int result=poll(&p,1,(int)left);
  if(result<0)return errno==EINTR; /* 交回主循环处理停止信号。 */
  if(!result)return 1;
  if(p.revents&(POLLERR|POLLHUP|POLLNVAL))return 0;
  char events[4096];ssize_t n=read(fd,events,sizeof events);
  if(n<0){if(errno==EAGAIN)continue;return errno==EINTR;}
  if(!n)return 0;
  int arm=0,wake=0;if(!po_command_event(events,(size_t)n,&arm,&wake))return 0;
  *pending_arm|=arm;if(wake)return 1;
 }
}
#endif
