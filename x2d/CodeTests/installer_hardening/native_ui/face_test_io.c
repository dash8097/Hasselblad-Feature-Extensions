#define _GNU_SOURCE
#include "face_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <unistd.h>
static unsigned checks;
#define C(x) do{++checks;if(!(x)){printf("FACE_IO_FAILED line=%d\n",__LINE__);reboot(RB_POWER_OFF);_exit(1);}}while(0)
static void put(const char*p,const char*s,size_t n){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,0644);C(f>=0);C(write(f,s,n)==(ssize_t)n);C(!close(f));}
static X2dFaceRequest request(int e,uint64_t id){X2dFaceRequest q={id,x2d_face_io_now_ms()+5000,e};return q;}
static X2dFaceReply run(X2dFaceRequest q){X2dFaceReply r;x2d_face_io_execute(&q,&r);C(r.id==q.id&&r.endpoint==q.endpoint);return r;}
int main(void){setvbuf(stdout,NULL,_IONBF,0);mount("proc","/proc","proc",0,NULL);C(!mkdir("/tmp",01777));C(!chmod("/tmp",01777));const char*p="/tmp/x2d-face-lock-backend.json";X2dFaceReply r;
 r=run(request(0,1));C(r.status==404&&r.length==0);put(p,"{}",2);r=run(request(0,1));C(r.status==200&&r.length==2&&!memcmp(r.body,"{}",2));
 char bytes[4097];memset(bytes,'a',sizeof bytes);put(p,bytes,4096);r=run(request(0,1));C(r.status==200&&r.length==4096);put(p,bytes,4097);r=run(request(0,1));C(r.status==413&&r.length==0);C(!unlink(p));
 C(!symlink("/proc/version",p));r=run(request(0,1));C(r.status==503);C(!unlink(p));C(!mkfifo(p,0600));r=run(request(0,1));C(r.status==503);C(!unlink(p));C(!mkdir(p,0700));r=run(request(0,1));C(r.status==503);C(!rmdir(p));
 put("/tmp/private","keep",4);C(!link("/tmp/private",p));r=run(request(0,1));C(r.status==503);C(!unlink(p));put(p,"{}",2);C(!chmod(p,0666));r=run(request(0,1));C(r.status==503);C(!chmod(p,0644));C(!chown(p,1000,1000));r=run(request(0,1));C(r.status==503);C(!chown(p,0,0));
 put("/tmp/x2d-eye-af-state.json","{\"protocol\":1}",14);r=run(request(1,2));C(r.status==200&&r.length==14);
 for(int e=-2;e<4;e++)if(e<0||e>1){r=run(request(e,1));C(r.status==400);}X2dFaceRequest q=request(0,0);r=run(q);C(r.status==400);q=request(0,1);q.deadline_ms=0;r=run(q);C(r.status==400);q.deadline_ms=1;r=run(q);C(r.status==408);
 C(!chmod("/tmp",0777));r=run(request(0,1));C(r.status==503);C(!chmod("/tmp",01777));
 X2dFaceIo*s=x2d_face_io_create();C(s);C(!x2d_face_io_poll(s,&r));
 for(unsigned batch=0;batch<128;batch++){for(unsigned i=0;i<4;i++){q=request(0,batch*4+i+1);C(x2d_face_io_submit(s,&q));C(!x2d_face_io_submit(s,&q));}q=request(0,999999);C(!x2d_face_io_submit(s,&q));for(unsigned i=0;i<4;i++){int done=0;for(unsigned n=0;n<500&&!done;n++){done=x2d_face_io_poll(s,&r);if(!done)usleep(1000);}C(done&&r.id==batch*4+i+1&&r.status==200&&r.length==2);}}
 q=request(0,10000);q.deadline_ms=1;C(x2d_face_io_submit(s,&q));int done=0;for(unsigned n=0;n<500&&!done;n++){done=x2d_face_io_poll(s,&r);if(!done)usleep(1000);}C(done&&r.status==408);
 C(!x2d_face_io_cancel(s,999999));x2d_face_io_stop(s);C(!x2d_face_io_submit(s,&q));int released=0;for(unsigned n=0;n<500&&!released;n++){released=x2d_face_io_release(s);if(!released)usleep(1000);}C(released);C(x2d_face_io_release(NULL));
 printf("FACE_IO_PASSED assertions=%u async_reads=512 camera_io=0\n",checks);reboot(RB_POWER_OFF);return 0;
}
