/* 4.2.0 实机 UI 测试输入：只使用已存在的触屏设备，不改变驱动或抓取设备。 */
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int emit(int fd,unsigned short type,unsigned short code,int value){
 struct input_event e={0};gettimeofday(&e.time,0);e.type=type;e.code=code;e.value=value;
 return write(fd,&e,sizeof e)==sizeof e;
}
static int number(const char*s,int high){char*end;errno=0;long n=strtol(s,&end,10);return errno||!*s||*end||n<0||n>high?-1:(int)n;}
int main(int argc,char**argv){
 int tap=argc==4&&!strcmp(argv[1],"tap");
 if(!tap&&(argc!=2||strcmp(argv[1],"inspect")))return 2;
 int x=tap?number(argv[2],1023):0,y=tap?number(argv[3],767):0;if(x<0||y<0)return 2;
 int fd=open("/dev/input/event8",(tap?O_RDWR:O_RDONLY)|O_CLOEXEC|O_NOFOLLOW);if(fd<0){perror("input-open");return 3;}
 char name[128]={0};struct input_absinfo ax={0},ay={0},slot={0};
 int rn=ioctl(fd,EVIOCGNAME(sizeof name),name),rx=ioctl(fd,EVIOCGABS(ABS_MT_POSITION_X),&ax),ry=ioctl(fd,EVIOCGABS(ABS_MT_POSITION_Y),&ay),rs=ioctl(fd,EVIOCGABS(ABS_MT_SLOT),&slot);
 printf("INPUT_INFO name=%s name_rc=%d x_rc=%d x=%d:%d y_rc=%d y=%d:%d slot_rc=%d slots=%d:%d\n",name,rn,rx,ax.minimum,ax.maximum,ry,ay.minimum,ay.maximum,rs,slot.minimum,slot.maximum);
 if(!tap){close(fd);return rn<0||rx||ry?4:0;}
 if(ioctl(fd,EVIOCGNAME(sizeof name),name)<0||strcmp(name,"himax-touchscreen")||
    ioctl(fd,EVIOCGABS(ABS_MT_POSITION_X),&ax)||ioctl(fd,EVIOCGABS(ABS_MT_POSITION_Y),&ay)||
    ioctl(fd,EVIOCGABS(ABS_MT_SLOT),&slot)||ax.maximum<=ax.minimum||ay.maximum<=ay.minimum||slot.minimum!=0){close(fd);return 4;}
 printf("TOUCH_DEVICE_VERIFIED name=%s x=%d:%d y=%d:%d slots=%d:%d\n",name,ax.minimum,ax.maximum,ay.minimum,ay.maximum,slot.minimum,slot.maximum);
 if(!tap){close(fd);return 0;}
 int rawx=ax.minimum+(long long)x*(ax.maximum-ax.minimum)/1023;
 int rawy=ay.minimum+(long long)y*(ay.maximum-ay.minimum)/767;
 int ok=emit(fd,EV_ABS,ABS_MT_SLOT,0)&&emit(fd,EV_ABS,ABS_MT_TRACKING_ID,500)&&
  emit(fd,EV_ABS,ABS_MT_POSITION_X,rawx)&&emit(fd,EV_ABS,ABS_MT_POSITION_Y,rawy)&&
  emit(fd,EV_KEY,BTN_TOUCH,1)&&emit(fd,EV_SYN,SYN_REPORT,0);
 usleep(80000);
 int released=emit(fd,EV_ABS,ABS_MT_SLOT,0)&&emit(fd,EV_ABS,ABS_MT_TRACKING_ID,-1)&&
  emit(fd,EV_KEY,BTN_TOUCH,0)&&emit(fd,EV_SYN,SYN_REPORT,0);
 close(fd);if(!ok||!released)return 5;
 printf("TOUCH_DELIVERED ui=%d,%d raw=%d,%d\n",x,y,rawx,rawy);return 0;
}
