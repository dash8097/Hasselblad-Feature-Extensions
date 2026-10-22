#define _GNU_SOURCE
#include "passive_output.h"
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
struct Slot{_Atomic int state,error;_Atomic uint64_t written,processed;uint64_t deadline;size_t size;char data[32768];};
struct X2dPassiveOutput{pthread_t thread;_Atomic int stop,done;struct Slot slots[2];};
static _Atomic unsigned long long sequence;
static const char*names[2]={"playback-live-status.json","ui.txt"};
static uint64_t now_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)||t.tv_sec<0)return 0;return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static int regular(const struct stat*s){return S_ISREG(s->st_mode)&&s->st_nlink==1&&s->st_uid==geteuid()&&!(s->st_mode&0022);}
static int same(const struct stat*a,const struct stat*b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino;}
static int expired(X2dPassiveOutput*w,const struct Slot*s){uint64_t n=now_ms();return atomic_load_explicit(&w->stop,memory_order_acquire)||!n||n>=s->deadline;}
static int output(X2dPassiveOutput*w,int kind){struct Slot*s=&w->slots[kind];int root=-1,dir=-1,fd=-1,error=0,created=0;char temp[96];struct stat parent,before,after,owned={0};
 if(expired(w,s))return ECANCELED;
 root=open("/tmp",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(root<0)goto fail;
 if(fstat(root,&parent))goto fail;if(!S_ISDIR(parent.st_mode)||(parent.st_uid!=0&&parent.st_uid!=geteuid())||((parent.st_mode&0022)&&!(parent.st_mode&S_ISVTX))){errno=EPERM;goto fail;}
 dir=openat(root,"x2d-pixel-overclock",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dir<0)goto fail;
 if(fstat(dir,&parent))goto fail;if(!S_ISDIR(parent.st_mode)||parent.st_uid!=geteuid()||(parent.st_mode&0022)){errno=EPERM;goto fail;}
 if(fstatat(dir,names[kind],&before,AT_SYMLINK_NOFOLLOW)==0){if(!regular(&before)){errno=EPERM;goto fail;}}else if(errno!=ENOENT)goto fail;
 snprintf(temp,sizeof temp,".x2d-passive-%ld-%llu",(long)getpid(),atomic_fetch_add(&sequence,1));fd=openat(dir,temp,O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0644);if(fd<0)goto fail;created=1;if(fstat(fd,&owned)||!regular(&owned)){errno=EPERM;goto fail;}
 size_t at=0;for(unsigned n=0;at<s->size&&n<64;n++){ssize_t z=write(fd,s->data+at,s->size-at);if(z<0){if(errno==EINTR)continue;goto fail;}if(!z){errno=EIO;goto fail;}at+=(size_t)z;}if(at!=s->size){errno=EIO;goto fail;}if(fsync(fd)||fstatat(dir,temp,&after,AT_SYMLINK_NOFOLLOW))goto fail;if(!regular(&after)||!same(&owned,&after)){errno=ESTALE;goto fail;}
 if(fstatat(root,"x2d-pixel-overclock",&after,AT_SYMLINK_NOFOLLOW)||!S_ISDIR(after.st_mode)||!same(&parent,&after)){errno=ESTALE;goto fail;}
 if(fstatat(dir,names[kind],&after,AT_SYMLINK_NOFOLLOW)==0){if(!regular(&after)){errno=EPERM;goto fail;}}else if(errno!=ENOENT)goto fail;
 if(expired(w,s)){errno=ECANCELED;goto fail;}if(renameat(dir,temp,dir,names[kind]))goto fail;created=0;goto done;
fail:error=errno?errno:EIO;
done:if(created&&fd>=0&&fstat(fd,&before)==0&&fstatat(dir,temp,&after,AT_SYMLINK_NOFOLLOW)==0&&same(&before,&after))unlinkat(dir,temp,0);if(fd>=0)close(fd);if(dir>=0)close(dir);if(root>=0)close(root);return error;
}
static void*worker(void*p){X2dPassiveOutput*w=p;while(!atomic_load_explicit(&w->stop,memory_order_acquire)){for(int k=0;k<2;k++){int expected=1;if(atomic_compare_exchange_strong_explicit(&w->slots[k].state,&expected,2,memory_order_acq_rel,memory_order_relaxed)){int e=output(w,k);atomic_store_explicit(&w->slots[k].error,e,memory_order_release);if(!e)atomic_fetch_add_explicit(&w->slots[k].written,1,memory_order_release);atomic_fetch_add_explicit(&w->slots[k].processed,1,memory_order_release);atomic_store_explicit(&w->slots[k].state,0,memory_order_release);}}struct timespec delay={0,10000000};nanosleep(&delay,NULL);}atomic_store_explicit(&w->done,1,memory_order_release);return NULL;}
X2dPassiveOutput*x2d_passive_create(void){X2dPassiveOutput*w=calloc(1,sizeof*w);if(!w)return NULL;atomic_init(&w->stop,0);atomic_init(&w->done,0);for(int k=0;k<2;k++){atomic_init(&w->slots[k].state,0);atomic_init(&w->slots[k].error,0);atomic_init(&w->slots[k].written,0);atomic_init(&w->slots[k].processed,0);}if(pthread_create(&w->thread,NULL,worker,w)){free(w);return NULL;}return w;}
int x2d_passive_publish(X2dPassiveOutput*w,int kind,const char*data,size_t size,uint64_t deadline){if(!w||kind<0||kind>1||!data||!size||size>(kind==0?32768:2048)||!deadline||memchr(data,0,size)||atomic_load_explicit(&w->stop,memory_order_acquire)||atomic_load_explicit(&w->done,memory_order_acquire))return 0;struct Slot*s=&w->slots[kind];int expected=atomic_load_explicit(&s->state,memory_order_acquire);if(expected!=0&&expected!=1)return 0;if(!atomic_compare_exchange_strong_explicit(&s->state,&expected,3,memory_order_acq_rel,memory_order_relaxed))return 0;memcpy(s->data,data,size);s->size=size;s->deadline=deadline;atomic_store_explicit(&s->state,1,memory_order_release);return 1;}
void x2d_passive_stop(X2dPassiveOutput*w){if(w)atomic_store_explicit(&w->stop,1,memory_order_release);}
int x2d_passive_release(X2dPassiveOutput*w){if(!w)return 1;if(!atomic_load_explicit(&w->done,memory_order_acquire))return 0;pthread_join(w->thread,NULL);free(w);return 1;}
uint64_t x2d_passive_written(const X2dPassiveOutput*w,int k){return w&&k>=0&&k<2?atomic_load_explicit(&w->slots[k].written,memory_order_acquire):0;}
uint64_t x2d_passive_processed(const X2dPassiveOutput*w,int k){return w&&k>=0&&k<2?atomic_load_explicit(&w->slots[k].processed,memory_order_acquire):0;}
int x2d_passive_error(const X2dPassiveOutput*w,int k){return w&&k>=0&&k<2?atomic_load_explicit(&w->slots[k].error,memory_order_acquire):EINVAL;}
