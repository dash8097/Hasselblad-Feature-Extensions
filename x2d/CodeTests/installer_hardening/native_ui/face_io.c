#define _GNU_SOURCE
#include "face_io.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static const char*const paths[]={"x2d-face-lock-backend.json","x2d-eye-af-state.json"};
uint64_t x2d_face_io_now_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static int valid(const X2dFaceRequest*q){return q&&q->id&&q->deadline_ms&&q->endpoint>=0&&q->endpoint<2;}
static int stale(const X2dFaceRequest*q,X2dFaceReply*r,const _Atomic int*c){if(c&&atomic_load_explicit(c,memory_order_acquire)){r->status=409;r->error=ECANCELED;return 1;}uint64_t n=x2d_face_io_now_ms();if(!n||n>=q->deadline_ms){r->status=408;r->error=ETIMEDOUT;return 1;}return 0;}
static int regular(const struct stat*s){return S_ISREG(s->st_mode)&&s->st_nlink==1&&s->st_uid==geteuid()&&!(s->st_mode&0022);}
static int identity(const struct stat*a,const struct stat*b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;}
static void execute(const X2dFaceRequest*q,X2dFaceReply*r,const _Atomic int*c){
 if(!r)return;memset(r,0,sizeof(*r));r->id=q?q->id:0;r->endpoint=q?q->endpoint:-1;r->status=400;if(!valid(q)||stale(q,r,c))return;
 int root=-1,fd=-1;struct stat before,after;root=open("/tmp",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(root<0)goto fail;
 if(fstat(root,&before)<0)goto fail;if(!S_ISDIR(before.st_mode)||(before.st_uid!=0&&before.st_uid!=geteuid())||((before.st_mode&0022)&&!(before.st_mode&S_ISVTX))){errno=EPERM;goto fail;}
 fd=openat(root,paths[q->endpoint],O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);if(fd<0)goto fail;
 if(fstat(fd,&before)<0)goto fail;if(!regular(&before)){errno=EINVAL;goto fail;}
 if(before.st_size<0||before.st_size>(off_t)sizeof(r->body)){r->status=413;goto done;}
 for(unsigned attempt=0;attempt<32;attempt++){
  if(stale(q,r,c))goto done;char extra;size_t available=sizeof(r->body)-r->length;ssize_t n=read(fd,available?r->body+r->length:&extra,available?available:1);
  if(n<0){if(errno==EINTR)continue;goto fail;}if(!n){
   if(fstat(fd,&after)<0)goto fail;if(!regular(&after)||!identity(&before,&after)||r->length!=(size_t)before.st_size){errno=EAGAIN;goto fail;}
   /* 名称已切换到另一个原子版本时拒绝陈旧快照；允许下次独立只读探测。 */
   if(fstatat(root,paths[q->endpoint],&after,AT_SYMLINK_NOFOLLOW)<0)goto fail;if(!regular(&after)||!identity(&before,&after)){errno=EAGAIN;goto fail;}
   if(stale(q,r,c))goto done;r->status=200;goto done;
  }if(!available){r->status=413;goto done;}r->length+=(size_t)n;
 }errno=EAGAIN;
fail:r->error=errno;r->status=errno==ENOENT?404:503;
done:if(r->status!=200)r->length=0;if(fd>=0)close(fd);if(root>=0)close(root);
}
void x2d_face_io_execute(const X2dFaceRequest*q,X2dFaceReply*r){execute(q,r,NULL);}
struct X2dFaceIo{_Atomic unsigned submitted,finished,collected;_Atomic int stopping,exited,canceled[4];X2dFaceRequest requests[4];X2dFaceReply replies[4];pthread_t thread;int joinable;};
static void*worker(void*p){X2dFaceIo*s=p;unsigned next=0;while(!atomic_load_explicit(&s->stopping,memory_order_acquire)){if(next==atomic_load_explicit(&s->submitted,memory_order_acquire)){usleep(2000);continue;}execute(&s->requests[next%4],&s->replies[next%4],&s->canceled[next%4]);++next;atomic_store_explicit(&s->finished,next,memory_order_release);}atomic_store_explicit(&s->exited,1,memory_order_release);return NULL;}
X2dFaceIo*x2d_face_io_create(void){X2dFaceIo*s=calloc(1,sizeof(*s));if(!s)return NULL;atomic_init(&s->submitted,0);atomic_init(&s->finished,0);atomic_init(&s->collected,0);atomic_init(&s->stopping,0);atomic_init(&s->exited,0);for(unsigned i=0;i<4;i++)atomic_init(&s->canceled[i],0);if(pthread_create(&s->thread,NULL,worker,s)){free(s);return NULL;}s->joinable=pthread_detach(s->thread)!=0;return s;}
int x2d_face_io_submit(X2dFaceIo*s,const X2dFaceRequest*q){if(!s||!valid(q)||atomic_load_explicit(&s->stopping,memory_order_acquire))return 0;unsigned at=atomic_load_explicit(&s->submitted,memory_order_relaxed),done=atomic_load_explicit(&s->collected,memory_order_relaxed);if(at-done>=4)return 0;for(unsigned i=done;i!=at;i++)if(s->requests[i%4].id==q->id)return 0;s->requests[at%4]=*q;atomic_store_explicit(&s->canceled[at%4],0,memory_order_relaxed);atomic_store_explicit(&s->submitted,at+1,memory_order_release);return 1;}
int x2d_face_io_poll(X2dFaceIo*s,X2dFaceReply*r){if(!s||!r)return 0;unsigned at=atomic_load_explicit(&s->collected,memory_order_relaxed);if(at==atomic_load_explicit(&s->finished,memory_order_acquire))return 0;*r=s->replies[at%4];atomic_store_explicit(&s->collected,at+1,memory_order_release);return 1;}
int x2d_face_io_cancel(X2dFaceIo*s,uint64_t id){if(!s||!id)return 0;unsigned at=atomic_load_explicit(&s->collected,memory_order_relaxed),end=atomic_load_explicit(&s->submitted,memory_order_relaxed);for(;at!=end;at++)if(s->requests[at%4].id==id){atomic_store_explicit(&s->canceled[at%4],1,memory_order_release);return 1;}return 0;}
void x2d_face_io_stop(X2dFaceIo*s){if(s){for(unsigned i=0;i<4;i++)atomic_store_explicit(&s->canceled[i],1,memory_order_release);atomic_store_explicit(&s->stopping,1,memory_order_release);}}
int x2d_face_io_release(X2dFaceIo*s){if(!s)return 1;x2d_face_io_stop(s);if(!atomic_load_explicit(&s->exited,memory_order_acquire))return 0;if(s->joinable&&pthread_join(s->thread,NULL))return 0;free(s);return 1;}
