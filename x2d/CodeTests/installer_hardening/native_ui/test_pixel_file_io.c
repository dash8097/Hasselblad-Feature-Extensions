#define _GNU_SOURCE
#include "pixel_file_io.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned assertions;
#define CHECK(x) do{++assertions;if(!(x)){printf("PIXEL_IO_FAILED line=%d\n",__LINE__);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}while(0)
static void put(const char*p,const char*s,size_t n){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,0644);CHECK(f>=0);CHECK(write(f,s,n)==(ssize_t)n);CHECK(!close(f));}
static X2dPixelIoRequest request(int endpoint,int write,const char*body){X2dPixelIoRequest q={0};q.id=42;q.endpoint=endpoint;q.write=write;if(body){q.length=strlen(body);memcpy(q.body,body,q.length);}return q;}
static X2dPixelIoReply execute(X2dPixelIoRequest q){X2dPixelIoReply r;x2d_pixel_io_execute(&q,&r);CHECK(r.id==q.id);return r;}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);mount("proc","/proc","proc",0,NULL);CHECK(!mkdir("/tmp",0777));CHECK(!mkdir("/tmp/x2d-pixel-overclock",0755));
 const char*p="/tmp/x2d-pixel-overclock/status.json";X2dPixelIoReply r;
 r=execute(request(0,0,NULL));CHECK(r.status==404&&r.length==0);
 put(p,"{}",2);r=execute(request(0,0,NULL));CHECK(r.status==200&&r.length==2&&!memcmp(r.body,"{}",2));
 char large[16385];memset(large,'x',sizeof large);put(p,large,16384);r=execute(request(0,0,NULL));CHECK(r.status==200&&r.length==16384);put(p,large,sizeof large);r=execute(request(0,0,NULL));CHECK(r.status==413&&r.length==0);
 CHECK(!unlink(p));CHECK(!symlink("/proc/version",p));r=execute(request(0,0,NULL));CHECK(r.status==503&&r.length==0);CHECK(!unlink(p));
 CHECK(!mkfifo(p,0600));r=execute(request(0,0,NULL));CHECK(r.status==503);CHECK(!unlink(p));
 CHECK(!mkdir(p,0700));r=execute(request(0,0,NULL));CHECK(r.status==503);CHECK(!rmdir(p));
 put("/tmp/private","keep",4);CHECK(!link("/tmp/private",p));r=execute(request(0,0,NULL));CHECK(r.status==503);CHECK(!unlink(p));
 put(p,"{}",2);put("/tmp/x2d-pixel-overclock/shutter-status.json","{\"ready\":true}",14);put("/tmp/x2d-quarter-native.status","QUARTER_NATIVE_READY\n",21);
 for(int i=0;i<3;i++){r=execute(request(i,0,NULL));CHECK(r.status==200);r=execute(request(i,1,"bad\n"));CHECK(r.status==400);}
 const char*ends[]={"arm.txt","quarter.txt","finish.txt"};char path[128];
 for(int e=3;e<6;e++){
  snprintf(path,sizeof(path),"/tmp/x2d-pixel-overclock/%s",ends[e-3]);
  r=execute(request(e,1,"DISARMED\n"));CHECK(r.status==200);r=execute(request(e,0,NULL));CHECK(r.status==200&&r.length==9&&!memcmp(r.body,"DISARMED\n",9));
  struct stat a,b;CHECK(!stat(path,&a));r=execute(request(e,1,"QN1 25 END\n"));CHECK(r.status==200);CHECK(!stat(path,&b)&&a.st_ino!=b.st_ino&&b.st_size==11);
  CHECK(!unlink(path));CHECK(!symlink("/tmp/private",path));r=execute(request(e,1,"DISARMED\n"));CHECK(r.status==503);CHECK(!unlink(path));
  CHECK(!link("/tmp/private",path));r=execute(request(e,1,"DISARMED\n"));CHECK(r.status==503);CHECK(!unlink(path));
  CHECK(!mkdir(path,0700));r=execute(request(e,1,"DISARMED\n"));CHECK(r.status==503);CHECK(!rmdir(path));
 }
 {char b[8]={0};int f=open("/tmp/private",O_RDONLY);CHECK(f>=0&&read(f,b,sizeof b)==4&&!memcmp(b,"keep",4));close(f);}
 for(int e=-2;e<9;e++)if(e<0||e>6){r=execute(request(e,0,NULL));CHECK(r.status==400);}
 /* 电源设置是独立持久化文件，不借用拍摄请求通道。 */
 r=execute(request(6,0,NULL));CHECK(r.status==404);CHECK(!mkdir("/blackbox",0755));
 const char*lv="/blackbox/x2d-liveview-timeout.txt";
 const char*choices[]={"X2DLV1 0\n","X2DLV1 10\n","X2DLV1 20\n","X2DLV1 30\n","X2DLV1 60\n","X2DLV1 120\n","X2DLV1 300\n"};
 for(unsigned i=0;i<7;i++){r=execute(request(6,1,choices[i]));CHECK(r.status==200);r=execute(request(6,0,NULL));CHECK(r.status==200&&r.length==strlen(choices[i])&&!memcmp(r.body,choices[i],r.length));}
 const char*invalid[]={"X2DLV1 1\n","X2DLV1 -1\n","X2DLV1 600\n","X2DLV1 030\n","X2DLV1 30 extra\n","DISARMED\n"};
 for(unsigned i=0;i<6;i++){r=execute(request(6,1,invalid[i]));CHECK(r.status==400);}
 CHECK(!unlink(lv));CHECK(!symlink("/tmp/private",lv));r=execute(request(6,1,choices[0]));CHECK(r.status==503);CHECK(!unlink(lv));
 CHECK(!rename("/blackbox","/blackbox-saved"));CHECK(!symlink("/blackbox-saved","/blackbox"));r=execute(request(6,1,choices[0]));CHECK(r.status==503);CHECK(!unlink("/blackbox"));CHECK(!rename("/blackbox-saved","/blackbox"));
 X2dPixelIoRequest q=request(3,1,"DISARMED\n");q.id=0;r=execute(q);CHECK(r.status==400);q.id=1;q.length=513;r=execute(q);CHECK(r.status==400);
 const char*bad[]={"","missing newline","two\nlines\n","nul","\tbad\n"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){r=execute(request(3,1,bad[i]));CHECK(r.status==400);}
 q=request(3,1,"DISARMED\n");q.body[2]=0;r=execute(q);CHECK(r.status==400);
 q=request(0,0,"unexpected");r=execute(q);CHECK(r.status==400);
 /* 父目录替换不跟随；没有创建缺失通道或写到链接目标。 */
 CHECK(!rename("/tmp/x2d-pixel-overclock","/tmp/saved"));CHECK(!symlink("/tmp/saved","/tmp/x2d-pixel-overclock"));r=execute(request(3,1,"DISARMED\n"));CHECK(r.status==503);CHECK(!unlink("/tmp/x2d-pixel-overclock"));
 r=execute(request(3,1,"DISARMED\n"));CHECK(r.status==404);CHECK(!rename("/tmp/saved","/tmp/x2d-pixel-overclock"));
 X2dPixelIo*s=x2d_pixel_io_create();CHECK(s);CHECK(!x2d_pixel_io_poll(s,&r));
 /* 未收集完成项仍占额度，不能覆盖结果；按id保持跨多轮队列次序。 */
 for(unsigned batch=0;batch<128;batch++){
  for(unsigned i=0;i<8;i++){q=request(0,0,NULL);q.id=batch*8+i+1;CHECK(x2d_pixel_io_submit(s,&q));}CHECK(!x2d_pixel_io_submit(s,&q));
  for(unsigned i=0;i<8;i++){int found=0;for(unsigned spins=0;spins<500&&!found;spins++){found=x2d_pixel_io_poll(s,&r);if(!found)usleep(1000);}CHECK(found&&r.id==batch*8+i+1&&r.status==200&&r.length==2);}
 }
 q=request(3,1,"DISARMED\n");q.id=10000;CHECK(x2d_pixel_io_submit(s,&q));int found=0;for(unsigned i=0;i<500&&!found;i++){found=x2d_pixel_io_poll(s,&r);if(!found)usleep(1000);}CHECK(found&&r.id==10000&&r.status==200);
 x2d_pixel_io_stop(s);CHECK(!x2d_pixel_io_submit(s,&q));int released=0;for(unsigned i=0;i<500&&!released;i++){released=x2d_pixel_io_release(s);if(!released)usleep(1000);}CHECK(released);CHECK(x2d_pixel_io_release(NULL));
 printf("PIXEL_IO_PASSED assertions=%u async_read_requests=1024\n",assertions);reboot(RB_POWER_OFF);return 0;
}
