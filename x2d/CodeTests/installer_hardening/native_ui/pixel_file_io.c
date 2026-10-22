#define _GNU_SOURCE
#include "pixel_file_io.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* 只访问固定 /tmp 通道。相机版本：第一代 X2D 4.2.0；不创建业务父目录。 */
static const char* names[]={"status.json","shutter-status.json","x2d-quarter-native.status","arm.txt","quarter.txt","finish.txt","x2d-liveview-timeout.txt"};
static _Atomic unsigned long long nonce;
static int valid(const X2dPixelIoRequest*q){
 if(!q||!q->id||q->endpoint<0||q->endpoint>X2D_LIVEVIEW_PREFERENCES||(q->write!=0&&q->write!=1))return 0;
 if(!q->write)return q->length==0;
 if(q->endpoint<3||!q->length||q->length>sizeof(q->body)||q->body[q->length-1]!='\n')return 0;
 for(size_t i=0;i+1<q->length;i++)if(q->body[i]<' '||q->body[i]>'~')return 0;
 if(q->endpoint==X2D_LIVEVIEW_PREFERENCES){
  static const char* values[]={"X2DLV1 0\n","X2DLV1 10\n","X2DLV1 20\n","X2DLV1 30\n","X2DLV1 60\n","X2DLV1 120\n","X2DLV1 300\n"};
  for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i)if(q->length==strlen(values[i])&&!memcmp(q->body,values[i],q->length))return 1;
  return 0;
 }
 return 1;
}
static int regular(const struct stat*s){return S_ISREG(s->st_mode)&&s->st_nlink==1&&s->st_uid==geteuid()&&!(s->st_mode&0022);}
static int identity(const struct stat*a,const struct stat*b){
 return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&
 a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
 a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
}
uint64_t x2d_pixel_io_now_ms(void){struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now))return 0;return (uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000;}
static int stale(const X2dPixelIoRequest*q,X2dPixelIoReply*r,const _Atomic int*cancel){
 if(cancel&&atomic_load_explicit(cancel,memory_order_acquire)){r->status=409;r->error=ECANCELED;return 1;}
 if(q->deadline_ms){uint64_t now=x2d_pixel_io_now_ms();if(!now||now>=q->deadline_ms){r->status=408;r->error=ETIMEDOUT;return 1;}}return 0;
}
static void execute(const X2dPixelIoRequest*q,X2dPixelIoReply*r,const _Atomic int*cancel){
 memset(r,0,sizeof(*r));r->id=q?q->id:0;r->status=400;
 if(!valid(q)||stale(q,r,cancel))return;
 int root=-1,dir=-1,fd=-1,created=0,saved=0;char temp[96]={0};struct stat before,after;
 root=open(q->endpoint==X2D_LIVEVIEW_PREFERENCES?"/blackbox":"/tmp",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(root<0)goto failure;
 if(fstat(root,&before)<0)goto failure;
 if(!S_ISDIR(before.st_mode)||(before.st_uid!=0&&before.st_uid!=geteuid())||((before.st_mode&0022)&&!(before.st_mode&S_ISVTX))){errno=EPERM;goto failure;}
 dir=openat(root,(q->endpoint==X2D_PIXEL_QUARTER_STATUS||q->endpoint==X2D_LIVEVIEW_PREFERENCES)?".":"x2d-pixel-overclock",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(dir<0)goto failure;
 if(q->endpoint!=X2D_PIXEL_QUARTER_STATUS&&q->endpoint!=X2D_LIVEVIEW_PREFERENCES){
  if(fstat(dir,&before)<0)goto failure;
  if(!S_ISDIR(before.st_mode)||before.st_uid!=geteuid()||(before.st_mode&0022)){errno=EPERM;goto failure;}
 }
 if(!q->write){
  fd=openat(dir,names[q->endpoint],O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);if(fd<0)goto failure;
  if(fstat(fd,&before)<0)goto failure;
  if(!regular(&before)){errno=EINVAL;goto failure;}
  if(before.st_size<0||before.st_size>(off_t)sizeof(r->body)){r->status=413;goto done;}
  for(unsigned operations=0;operations<32;operations++){
   char extra;size_t available=sizeof(r->body)-r->length;
   ssize_t n=read(fd,available?r->body+r->length:&extra,available?available:1);
   if(n<0){if(errno==EINTR)continue;goto failure;}
   if(!n){
    if(fstat(fd,&after)<0)goto failure;
    if(!regular(&after)||!identity(&before,&after)||r->length!=(size_t)before.st_size){errno=EAGAIN;goto failure;}
    r->status=200;goto done;
   }
   if(!available){r->status=413;goto done;}r->length+=(size_t)n;
  }
  errno=EAGAIN;goto failure;
 }
 /* 旧文件必须是独立常规文件；rename 不会跟随并发替换的最终符号链接。 */
 if(fstatat(dir,names[q->endpoint],&before,AT_SYMLINK_NOFOLLOW)==0){if(!regular(&before)){errno=EINVAL;goto failure;}}
 else if(errno!=ENOENT)goto failure;
 snprintf(temp,sizeof(temp),q->endpoint==X2D_LIVEVIEW_PREFERENCES?"x2d-liveview-timeout-%ld-%llu.next":".x2d-ui-%ld-%llu",(long)getpid(),atomic_fetch_add(&nonce,1));
 fd=openat(dir,temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0644);if(fd<0)goto failure;created=1;
 {size_t at=0;for(unsigned operations=0;at<q->length&&operations<32;operations++){
   ssize_t n=write(fd,q->body+at,q->length-at);if(n<0){if(errno==EINTR)continue;goto failure;}if(!n){errno=EIO;goto failure;}at+=(size_t)n;
  }if(at!=q->length){errno=EAGAIN;goto failure;}}
 if(fsync(fd)<0)goto failure;
 if(fstat(fd,&before)<0||fstatat(dir,temp,&after,AT_SYMLINK_NOFOLLOW)<0)goto failure;
 if(!regular(&before)||!regular(&after)||!identity(&before,&after)){errno=ESTALE;goto failure;}
 if(stale(q,r,cancel))goto done;
 if(renameat(dir,temp,dir,names[q->endpoint])<0)goto failure;created=0;
 if(q->endpoint==X2D_LIVEVIEW_PREFERENCES&&fsync(dir)<0)goto failure;
 /* 通道在 tmpfs，不承诺跨断电持久化；成功仅表示原子发布，后端接受另行读回。 */
 r->status=200;goto done;
failure:
 saved=errno;r->status=saved==ENOENT?404:503;r->error=saved;
done:
 if(r->status!=200)r->length=0;
 if(created&&fd>=0&&fstat(fd,&before)==0&&fstatat(dir,temp,&after,AT_SYMLINK_NOFOLLOW)==0&&before.st_dev==after.st_dev&&before.st_ino==after.st_ino)unlinkat(dir,temp,0);
 if(fd>=0)close(fd);if(dir>=0)close(dir);if(root>=0)close(root);
}
void x2d_pixel_io_execute(const X2dPixelIoRequest*q,X2dPixelIoReply*r){execute(q,r,NULL);}

/* 两条 SPSC 环形队列共享8个额度。GUI永不等待磁盘或工作线程。 */
struct X2dPixelIo {
 _Atomic unsigned submitted,finished,collected;
 _Atomic int stopping,exited;
 _Atomic int canceled[8];
 X2dPixelIoRequest requests[8];X2dPixelIoReply replies[8];
 pthread_t thread;int joinable;sem_t wake;
};
static void* worker(void*arg){
 X2dPixelIo*s=arg;unsigned next=0;
 while(!atomic_load_explicit(&s->stopping,memory_order_acquire)){
  while(sem_wait(&s->wake)<0&&errno==EINTR){}
  if(atomic_load_explicit(&s->stopping,memory_order_acquire))break;
  if(next==atomic_load_explicit(&s->submitted,memory_order_acquire))continue;
  execute(&s->requests[next%8],&s->replies[next%8],&s->canceled[next%8]);++next;
  atomic_store_explicit(&s->finished,next,memory_order_release);
 }
 atomic_store_explicit(&s->exited,1,memory_order_release);return NULL;
}
X2dPixelIo* x2d_pixel_io_create(void){
 X2dPixelIo*s=calloc(1,sizeof(*s));if(!s)return NULL;
 atomic_init(&s->submitted,0);atomic_init(&s->finished,0);atomic_init(&s->collected,0);
 atomic_init(&s->stopping,0);atomic_init(&s->exited,0);
 for(unsigned i=0;i<8;i++)atomic_init(&s->canceled[i],0);
 if(sem_init(&s->wake,0,0)){free(s);return NULL;}
 if(pthread_create(&s->thread,NULL,worker,s)){sem_destroy(&s->wake);free(s);return NULL;}
 s->joinable=pthread_detach(s->thread)!=0;return s;
}
int x2d_pixel_io_submit(X2dPixelIo*s,const X2dPixelIoRequest*q){
 if(!s||!valid(q)||atomic_load_explicit(&s->stopping,memory_order_acquire))return 0;
 unsigned at=atomic_load_explicit(&s->submitted,memory_order_relaxed),done=atomic_load_explicit(&s->collected,memory_order_relaxed);
 if(at-done>=8)return 0;
 s->requests[at%8]=*q;if(!s->requests[at%8].deadline_ms){uint64_t now=x2d_pixel_io_now_ms();if(!now)return 0;s->requests[at%8].deadline_ms=now+5000;}
 atomic_store_explicit(&s->canceled[at%8],0,memory_order_relaxed);atomic_store_explicit(&s->submitted,at+1,memory_order_release);sem_post(&s->wake);return 1;
}
int x2d_pixel_io_poll(X2dPixelIo*s,X2dPixelIoReply*r){
 if(!s||!r)return 0;unsigned at=atomic_load_explicit(&s->collected,memory_order_relaxed);
 if(at==atomic_load_explicit(&s->finished,memory_order_acquire))return 0;
 *r=s->replies[at%8];atomic_store_explicit(&s->collected,at+1,memory_order_release);return 1;
}
int x2d_pixel_io_cancel(X2dPixelIo*s,uint64_t id){
 if(!s||!id)return 0;unsigned at=atomic_load_explicit(&s->collected,memory_order_relaxed),end=atomic_load_explicit(&s->submitted,memory_order_relaxed);
 for(;at!=end;at++)if(s->requests[at%8].id==id){atomic_store_explicit(&s->canceled[at%8],1,memory_order_release);return 1;}return 0;
}
void x2d_pixel_io_stop(X2dPixelIo*s){if(s){for(unsigned i=0;i<8;i++)atomic_store_explicit(&s->canceled[i],1,memory_order_release);if(!atomic_exchange_explicit(&s->stopping,1,memory_order_acq_rel))sem_post(&s->wake);}}
int x2d_pixel_io_release(X2dPixelIo*s){
 if(!s)return 1;x2d_pixel_io_stop(s);if(!atomic_load_explicit(&s->exited,memory_order_acquire))return 0;
 if(s->joinable&&pthread_join(s->thread,NULL))return 0;sem_destroy(&s->wake);free(s);return 1;
}
