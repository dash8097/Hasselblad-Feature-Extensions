/* 候选：以目录变更通知等待原有完成文件，保留单调截止时间。
 * 未接入稳定启动脚本。只读取状态；--selftest 只操作自己的临时目录。
 */
#define _GNU_SOURCE
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int64_t now_ms(void) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)) return -1;
    return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
/* 0=等待，1=就绪，负数=文件异常/明确失败。 */
static int state(int directory,const char *name,const char *expected,int prefix) {
    int fd=openat(directory,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0) return errno==ENOENT?0:-1;
    struct stat st;char bytes[513];
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() || st.st_nlink>1 ||
       st.st_size<0 || st.st_size>512) {close(fd);return -1;}
    // QSaveFile 原子替换后，已打开的旧文件可能暂时没有目录链接。
    // 这是正常竞争；等待已排队的目录通知后重新打开，不接受旧结果。
    if(st.st_nlink==0) {close(fd);return 0;}
    ssize_t n=read(fd,bytes,512);int saved=errno;close(fd);
    if(n<0) return saved==EINTR?0:-1;
    if(memchr(bytes,0,(size_t)n)) return -1;
    while(n>0 && (bytes[n-1]=='\n' || bytes[n-1]=='\r')) --n;
    bytes[n]=0;
    size_t wanted=strlen(expected);
    if((prefix && (size_t)n>=wanted && !memcmp(bytes,expected,wanted)) ||
       (!prefix && !strcmp(bytes,expected))) return 1;
    if(!strncmp(bytes,"state=failed",12) || !strncmp(bytes,"state=blocked",13) ||
       (n>=7 && !strcmp(bytes+n-7," failed"))) return -2;
    return 0;
}
static int wait_status(const char *directory,const char *name,const char *expected,int prefix,unsigned timeout) {
    int result=80;
    int d=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(d<0) return result;
    struct stat st;
    if(fstat(d,&st) || st.st_uid!=geteuid() || (st.st_mode&0777)!=0700) {close(d);return result;}
    int notify=inotify_init1(IN_CLOEXEC|IN_NONBLOCK);
    if(notify<0) {close(d);return result;}
    /* 用已打开的目录添加监听，避免目录替换导致读和监听不同对象。 */
    char reference[64];snprintf(reference,sizeof(reference),"/proc/self/fd/%d",d);
    if(inotify_add_watch(notify,reference,IN_CLOSE_WRITE|IN_MOVED_TO|IN_CREATE|IN_DELETE_SELF|IN_MOVE_SELF)<0) goto done;
    int64_t start=now_ms();
    if(start<0) goto done;
    for(;;) {
        int status=state(d,name,expected,prefix);
        if(status==1) {result=0;break;}
        if(status<0) {result=status==-2?83:80;break;}
        int64_t current=now_ms();
        if(current<start) break;
        int64_t left=(int64_t)timeout-(current-start);
        if(left<=0) {result=84;break;}
        struct pollfd p={notify,POLLIN,0};
        int rc=poll(&p,1,(int)left);
        if(rc<0) {if(errno==EINTR) continue;break;}
        if(!rc) continue;
        if(p.revents&(POLLERR|POLLHUP|POLLNVAL)) break;
        char events[4096] __attribute__((aligned(8)));
        ssize_t n=read(notify,events,sizeof(events));
        if(n<0) {if(errno==EAGAIN || errno==EINTR) continue;break;}
        if(!n) break;
        int invalid=0;
        for(size_t at=0;at+sizeof(struct inotify_event)<=(size_t)n;) {
            struct inotify_event *event=(struct inotify_event *)(events+at);
            if(event->mask&(IN_DELETE_SELF|IN_MOVE_SELF|IN_IGNORED)) invalid=1;
            at+=sizeof(*event)+event->len;
        }
        if(invalid) break;
    }
done:
    close(notify);close(d);return result;
}
static int put(const char *path,const char *text) {
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if(fd<0) return 0;
    size_t n=strlen(text);int ok=write(fd,text,n)==(ssize_t)n;
    if(close(fd)) ok=0;
    return ok;
}
static int selftest(void) {
    char directory[]="/tmp/hbl-boot-wait-XXXXXX";
    if(!mkdtemp(directory)) return 90;
    char path[128],next[128];snprintf(path,sizeof(path),"%s/state",directory);snprintf(next,sizeof(next),"%s/next",directory);
    int ok=put(path,"ready\n") && wait_status(directory,"state","ready",0,100)==0;
    unlink(path);
    int64_t began=now_ms();
    pid_t child=fork();
    if(child==0) {usleep(20000);_exit(put(next,"ready\n") && rename(next,path)==0?0:1);}
    if(child<0) ok=0;
    else {
        int status=0;
        ok=(wait_status(directory,"state","ready",0,2000)==0)&&ok;
        ok=(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0)&&ok;
    }
    int64_t notified=now_ms()-began;
    unlink(path);unlink(next);
    ok=(wait_status(directory,"state","ready",0,30)==84)&&ok;
    ok=put(path,"state=failed reason=test\n")&&ok;
    ok=(wait_status(directory,"state","state=ready ",1,100)==83)&&ok;
    unlink(path);
    ok=(symlink("/does-not-exist",path)==0)&&ok;
    ok=(wait_status(directory,"state","ready",0,100)==80)&&ok;
    unlink(path);rmdir(directory);
    printf("boot-wait-selftest passed=%d notificationMs=%lld hardware=0\n",ok,(long long)notified);
    return ok?0:91;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--selftest")) return selftest();
    if(argc==2 && !strcmp(argv[1],"--loader")) return wait_status("/run/hbl-four-module","boot-loader.status","state=ready phase=modules-ready ",1,390000);
    if(argc==2 && !strcmp(argv[1],"--controls")) return wait_status("/tmp/hbl-wireless-flash","formal-enable.confirmed","ready",0,15000);
    if(argc==3 && !strcmp(argv[1],"--settings")) {
        char *end=0;errno=0;unsigned long pid=strtoul(argv[2],&end,10);
        if(errno || !argv[2][0] || *end || !pid || pid>2147483647) return 80;
        char expected[64];snprintf(expected,sizeof(expected),"HPR1 %lu ready",pid);
        return wait_status("/run/hbl-four-module","settings-restore.status",expected,0,15000);
    }
    return 80;
}
