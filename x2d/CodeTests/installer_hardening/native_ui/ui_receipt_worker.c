#define _GNU_SOURCE
#include "ui_receipt.h"
#ifdef X2D_UI_VERIFY_LOADED
#include "loaded_module_identity.h"
#endif
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
struct X2dUiReceiptWorker {pthread_t thread;_Atomic int slot,stop,done;_Atomic uint64_t generation;X2dUiQuery query;X2dUiReceipt reply;char build[65],gui[65];int verify_loaded;};
static int regular(const struct stat*s){return S_ISREG(s->st_mode)&&s->st_nlink==1&&s->st_uid==geteuid()&&!(s->st_mode&0022);}
static int same(const struct stat*a,const struct stat*b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;}
static int tmpdir(void){int fd=open("/tmp",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);struct stat s;if(fd<0)return -1;if(fstat(fd,&s)||!S_ISDIR(s.st_mode)||(s.st_uid!=0&&s.st_uid!=geteuid())||((s.st_mode&0022)&&!(s.st_mode&S_ISVTX))){close(fd);return -1;}return fd;}
static int query_read(int dir,char nonce[33]){int fd=openat(dir,"x2d-native-ui.query",O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return 0;struct stat a,b;char text[46];size_t used=0;int ok=0;
 if(fstat(fd,&a)||!regular(&a)||a.st_size!=45)goto done;
 for(unsigned tries=0;tries<16;tries++){ssize_t n=read(fd,text+used,sizeof text-used);if(n<0){if(errno==EINTR)continue;goto done;}if(!n)break;used+=(size_t)n;if(used==sizeof text)goto done;}
 if(used!=45||fstat(fd,&b)||!regular(&b)||!same(&a,&b))goto done;ok=x2d_ui_query_parse(text,45,nonce);
done:close(fd);return ok;}
static uint64_t start_ticks(void){int fd=open("/proc/self/stat",O_RDONLY|O_CLOEXEC);if(fd<0)return 0;char data[4096];ssize_t n=read(fd,data,sizeof data-1);close(fd);if(n<=0)return 0;data[n]=0;char*p=strrchr(data,')');if(!p||p[1]!=' ')return 0;p+=2;
 for(unsigned field=3;field<22;field++){p=strchr(p,' ');if(!p)return 0;++p;}char*end;errno=0;unsigned long long v=strtoull(p,&end,10);return errno||end==p||(*end!=' '&&*end!='\n')?0:v;}
static int write_receipt(int dir,const X2dUiReceipt*r,unsigned long long seq){char text[512],name[96];size_t size=x2d_ui_receipt_format(r,text,sizeof text);if(!size)return 0;struct stat before={0},after;
 if(fstatat(dir,"x2d-native-ui.status",&before,AT_SYMLINK_NOFOLLOW)==0){if(!regular(&before))return 0;}else if(errno!=ENOENT)return 0;
 snprintf(name,sizeof name,".x2d-native-ui-%ld-%llu",(long)getpid(),seq);int fd=openat(dir,name,O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return 0;int ok=0;size_t used=0;
 if(fstat(fd,&before)||!regular(&before))goto done;
 for(unsigned tries=0;used<size&&tries<32;tries++){ssize_t n=write(fd,text+used,size-used);if(n<0){if(errno==EINTR)continue;goto done;}if(!n)goto done;used+=(size_t)n;}
 if(used!=size||fsync(fd)||fstatat(dir,name,&after,AT_SYMLINK_NOFOLLOW)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||!regular(&after))goto done;
 ok=renameat(dir,name,dir,"x2d-native-ui.status")==0;
done:close(fd);if(!ok&&fstatat(dir,name,&after,AT_SYMLINK_NOFOLLOW)==0&&after.st_dev==before.st_dev&&after.st_ino==before.st_ino)unlinkat(dir,name,0);return ok;}
static void*worker(void*arg){X2dUiReceiptWorker*w=arg;int dir=tmpdir();uint64_t start=start_ticks();char last[33]={0};unsigned long long seq=0;
 if(dir<0||!start)goto finish;
#ifdef X2D_UI_VERIFY_LOADED
 if(w->verify_loaded&&!x2d_loaded_module_hash(w->build))goto finish;
#endif
 while(!atomic_load_explicit(&w->stop,memory_order_acquire)){
  int state=atomic_load_explicit(&w->slot,memory_order_acquire);
  if(state==0){char nonce[33];if(query_read(dir,nonce)&&strcmp(nonce,last)){memcpy(w->query.nonce,nonce,33);w->query.generation=atomic_load_explicit(&w->generation,memory_order_acquire);atomic_store_explicit(&w->slot,1,memory_order_release);}}
  else if(state==3){
#ifdef X2D_UI_VERIFY_LOADED
   if(w->verify_loaded){char fresh[65];if(!x2d_loaded_module_hash(fresh)||strcmp(fresh,w->build))goto finish;}
#endif
   X2dUiReceipt reply=w->reply;reply.pid=(uint64_t)getpid();reply.start_ticks=start;memcpy(reply.build,w->build,65);memcpy(reply.gui,w->gui,65);
   uint64_t generation=atomic_load_explicit(&w->generation,memory_order_acquire);if(generation!=reply.generation){reply.generation=generation;reply.state=X2D_UI_STOPPED;reply.capabilities=0;}
   char current[33];if(query_read(dir,current)&&!strcmp(current,reply.nonce)&&!atomic_load_explicit(&w->stop,memory_order_acquire))write_receipt(dir,&reply,++seq);
   memcpy(last,reply.nonce,33);atomic_store_explicit(&w->slot,0,memory_order_release);
  }
  struct timespec pause={0,10000000};nanosleep(&pause,NULL);
 }
finish:if(dir>=0)close(dir);atomic_store_explicit(&w->done,1,memory_order_release);return NULL;
}
static X2dUiReceiptWorker*create(const char*build,const char*gui,int verify_loaded){
 X2dUiReceipt sample={.pid=1,.start_ticks=1,.generation=1,.sampled_ms=1,.state=X2D_UI_ATTACHING};char text[512];
 if(!build||!gui||strnlen(build,65)!=64||strnlen(gui,65)!=64)return NULL;memset(sample.nonce,'0',32);memcpy(sample.build,build,65);memcpy(sample.gui,gui,65);if(!x2d_ui_receipt_format(&sample,text,sizeof text))return NULL;
 X2dUiReceiptWorker*w=calloc(1,sizeof*w);if(!w)return NULL;w->verify_loaded=verify_loaded;memcpy(w->build,build,65);memcpy(w->gui,gui,65);atomic_init(&w->generation,1);atomic_init(&w->slot,0);atomic_init(&w->stop,0);atomic_init(&w->done,0);if(pthread_create(&w->thread,NULL,worker,w)){free(w);return NULL;}return w;
}
X2dUiReceiptWorker*x2d_ui_receipt_create(const char*build,const char*gui){return create(build,gui,0);}
X2dUiReceiptWorker*x2d_ui_receipt_create_loaded(const char*gui){
#ifdef X2D_UI_VERIFY_LOADED
 return create("0000000000000000000000000000000000000000000000000000000000000000",gui,1);
#else
 (void)gui;return NULL;
#endif
}
int x2d_ui_receipt_take_query(X2dUiReceiptWorker*w,X2dUiQuery*q){if(!w||!q||atomic_load_explicit(&w->stop,memory_order_acquire)||atomic_load_explicit(&w->done,memory_order_acquire))return 0;int expected=1;if(!atomic_compare_exchange_strong_explicit(&w->slot,&expected,2,memory_order_acq_rel,memory_order_relaxed))return 0;*q=w->query;q->generation=atomic_load_explicit(&w->generation,memory_order_acquire);return 1;}
int x2d_ui_receipt_reply(X2dUiReceiptWorker*w,const X2dUiQuery*q,int state,uint64_t caps,uint64_t now){
 if(!w||!q||!now||state<1||state>4||atomic_load_explicit(&w->stop,memory_order_acquire)||atomic_load_explicit(&w->slot,memory_order_acquire)!=2||memcmp(w->query.nonce,q->nonce,33))return 0;
 memset(&w->reply,0,sizeof w->reply);memcpy(w->reply.nonce,q->nonce,33);w->reply.generation=q->generation;w->reply.sampled_ms=now;w->reply.capabilities=caps;w->reply.state=state;atomic_store_explicit(&w->slot,3,memory_order_release);return 1;
}
void x2d_ui_receipt_invalidate(X2dUiReceiptWorker*w){if(!w)return;uint64_t generation=atomic_fetch_add_explicit(&w->generation,1,memory_order_acq_rel)+1;
 /* Single GUI producer: losing its root may abandon a taken query. Publish a
  * negative reply so slot=2 cannot permanently strand the next root/session. */
 if(atomic_load_explicit(&w->slot,memory_order_acquire)==2){struct timespec t;uint64_t ms=1;if(!clock_gettime(CLOCK_MONOTONIC,&t))ms=(uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
  memset(&w->reply,0,sizeof w->reply);memcpy(w->reply.nonce,w->query.nonce,33);w->reply.generation=generation;w->reply.sampled_ms=ms?ms:1;w->reply.state=X2D_UI_STOPPED;atomic_store_explicit(&w->slot,3,memory_order_release);}
}
void x2d_ui_receipt_stop(X2dUiReceiptWorker*w){if(w){x2d_ui_receipt_invalidate(w);atomic_store_explicit(&w->stop,1,memory_order_release);}}
int x2d_ui_receipt_release(X2dUiReceiptWorker*w){if(!w)return 1;if(!atomic_load_explicit(&w->done,memory_order_acquire))return 0;pthread_join(w->thread,NULL);free(w);return 1;}
