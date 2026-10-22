/* 无 system/popen/shell；固定 argv 启动原有编码器或音频宿主。 */
#define _GNU_SOURCE
#include "camera_native.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static volatile sig_atomic_t interrupted;
static volatile sig_atomic_t recovering;
int cn_cancel_requested(void) { return interrupted && !recovering; }
int cn_signal_pending(void) { return interrupted; }
void cn_recovery_mode(int active) { if(active)recovering++;else if(recovering)recovering--; }
#ifdef CN_FULL_FLOW
extern int cn_counter_main(int,char**);
extern int cn_publish_main(int,char**);
extern int cn_render_main(int,char**);
extern int cn_prepare_main(int,char**);
extern int cn_heif_restart_main(int,char**);
extern int cn_ui_check_main(int,char**);
extern int cn_setup_main(int,char**);
extern int cn_uninstall_main(int,char**);
extern int cn_install_main(int,char**);
extern int cn_data_cleanup_main(int,char**);
extern int cn_memory_runner_main(int,char**);
extern int cn_disk_runner_main(int,char**);
extern int cn_launch_main(int,char**);
extern int cn_stack_runner_main(int,char**);
extern int cn_dispatch_main(int,char**);
extern int cn_transaction_read_main(int,char**);
#ifdef CN_PACKAGED_PAYLOAD
extern int cn_unpack_main(int,char**);
extern int cn_bootstrap_main(int,char**);
#endif
#endif
extern char **environ;
struct context { int job; dev_t device; ino_t inode; };
static void on_signal(int n) { (void)n;interrupted=1; }
static int cancelled(void *c) { (void)c;return interrupted!=0; }
static void event(void *c,const char *s) { (void)c;puts(s); }
static int failed(const char *step,int cause) {
    fprintf(stderr,"NATIVE_WRAPPER_FAILURE step=%s errno=%d\n",step,cause);return 1;
}

/* 逐分量打开目录，拒绝父目录链接；不会创建 SSD 或任务目录。 */
static int directory(const char *path) {
    if(!path || path[0]!='/') {errno=EINVAL;return -1;}
    int fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(fd<0) return -1;
    const char *p=path+1;
    while(*p) {
        const char *end=strchr(p,'/');size_t n=end?(size_t)(end-p):strlen(p);
        char part[128];
        if(!n || n>=sizeof(part)) {close(fd);errno=EINVAL;return -1;}
        memcpy(part,p,n);part[n]=0;
        if(!strcmp(part,".") || !strcmp(part,"..")) {close(fd);errno=EINVAL;return -1;}
        int next=openat(fd,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        int cause=errno;close(fd);fd=next;errno=cause;
        if(fd<0) return -1;
        if(!end) break;
        p=end+1;
    }
    return fd;
}
static int regular(int dir,const char *name,int flags,mode_t mode) {
    int fd=openat(dir,name,flags|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,mode);
    if(fd<0) return -1;
    struct stat st;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_nlink!=1) {
        close(fd);errno=EINVAL;return -1;
    }
    return fd;
}
static int absent(int dir,const char *name) {
    struct stat st;return fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT;
}
static int exact(int dir,const char *name,const char *expected) {
    char text[128];int fd=regular(dir,name,O_RDONLY,0);
    if(fd<0) return 0;
    ssize_t n=read(fd,text,sizeof(text));int cause=errno;close(fd);errno=cause;
    if(n<0 || n==(ssize_t)sizeof(text) || memchr(text,0,(size_t)n)) return 0;
    while(n>0 && text[n-1]=='\n') n--;
    return (size_t)n==strlen(expected) && !memcmp(text,expected,(size_t)n);
}
static int prepare(void *opaque,const char *job) {
    struct context *c=opaque;c->job=directory(job);
    if(c->job<0) return failed("open-job",errno);
    struct stat st;
    if(fstat(c->job,&st)) return failed("stat-job",errno);
    c->device=st.st_dev;c->inode=st.st_ino;
    if(!exact(c->job,"native.disk.ready","NATIVE_DISK_RAW_JPEG_DURABLE")) return failed("durable-marker",EINVAL);
    int fd=regular(c->job,"album.jpg",O_RDONLY,0);
    if(fd<0) return failed("open-jpeg",errno);
    int ok=!fstat(fd,&st) && st.st_size>0;close(fd);
    return ok && absent(c->job,"album.heic") && absent(c->job,"heif.ready")?0:failed("input-or-output-state",EINVAL);
}
static int memory(void *c,uint64_t *out) {
    (void)c;char text[16384];int fd=open("/proc/meminfo",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0) return 0;
    size_t used=0;
    for(;;) {
        ssize_t n=read(fd,text+used,sizeof(text)-used);
        if(n<0 && errno==EINTR) continue;
        if(n<0) {close(fd);return 0;}
        if(!n) break;
        used+=(size_t)n;if(used==sizeof(text)) {close(fd);return 0;}
    }
    close(fd);return cn_mem_available(text,used,out);
}
static int delay(void *c) {
    (void)c;struct timespec remaining={0,200000000};
    while(nanosleep(&remaining,&remaining)) {
        if(errno!=EINTR || interrupted) return 0;
    }
    return !interrupted;
}
static int redirect(int fd) {
    /* init 即使关闭了标准句柄也必须正确；dup2(fd,fd) 不清 CLOEXEC。 */
    int copy=fcntl(fd,F_DUPFD_CLOEXEC,3);
    if(copy<0) return 0;
    int ok=dup2(copy,1)>=0 && dup2(copy,2)>=0;
    close(copy);return ok;
}
static void close_log(int fd) { if(fd!=1 && fd!=2) close(fd); }
static int encode(void *opaque,const char *job) {
    struct context *c=opaque;
    int current=directory(job);struct stat st;
    int same=current>=0 && !fstat(current,&st) && st.st_dev==c->device && st.st_ino==c->inode;
    if(current>=0) close(current);
    if(!same || !absent(c->job,"album.heic") || !absent(c->job,"heif.ready")) return failed("recheck-job",EINVAL);
    /* 先检查类型再截断，避免通过硬链接破坏其他任务的日志。 */
    int log=regular(c->job,"heif.log",O_WRONLY|O_CREAT,0600);
    if(log<0) return failed("open-encoder-log",errno);
    if(ftruncate(log,0)) {int cause=errno;close(log);return failed("truncate-encoder-log",cause);}
    pid_t child=fork();
    if(child<0) {int cause=errno;close(log);return failed("fork-encoder",cause);}
    if(!child) {
        signal(SIGHUP,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);
        if(!redirect(log)) _exit(126);
        close_log(log);
        /* 从已打开的任务目录启动，固定程序不解释任务路径为命令。 */
        if(fchdir(c->job)) _exit(126);
        char *args[]={"/system/bin/x2d-pixel-overclock/heif",(char *)job,NULL};
        execve(args[0],args,environ);failed("exec-encoder",errno);_exit(127);
    }
    close(log);int status;
    /* 信号只改变最终返回值；像旧 wrapper 一样等待本次子进程，不杀原厂服务。 */
    while(waitpid(child,&status,0)<0) {if(errno!=EINTR) return failed("wait-encoder",errno);}
    if(WIFEXITED(status)) return WEXITSTATUS(status);
    if(WIFSIGNALED(status)) return 128+WTERMSIG(status);
    return 1;
}
static int verify(void *opaque) {
    struct context *c=opaque;int fd=regular(c->job,"heif.log",O_RDONLY,0);
    if(fd<0) return failed("read-encoder-log",errno);
    const char needle[]="x265 [error]";size_t matched=0,total=0;char block[4096];int ok=1;
    for(;;) {
        ssize_t n=read(fd,block,sizeof(block));
        if(n<0 && errno==EINTR) continue;
        if(n<0) {ok=0;break;}
        if(!n) break;
        total+=(size_t)n;
        if(total>16u*1024u*1024u) {ok=0;break;}
        for(ssize_t i=0;i<n;i++) {
            if(block[i]==needle[matched]) matched++;
            else matched=block[i]==needle[0]?1:0;
            if(matched==sizeof(needle)-1) {ok=0;break;}
        }
        if(!ok) break;
    }
    close(fd);
    if(!ok) return failed("encoder-log-error-or-unreadable",EINVAL);
    return exact(c->job,"heif.ready","HEIF_ENCODED 23326 17498 15")?0:failed("encoder-ready-marker",EINVAL);
}
static int audio(void *c) {
    (void)c;if(interrupted) return 143;
    int dir=directory("/tmp");
    if(dir<0) return failed("audio-open-tmp",errno);
    int pid=regular(dir,"x2d-shutter-audio.pid",O_WRONLY|O_CREAT,0600);
    if(pid<0) {int cause=errno;close(dir);return failed("audio-open-pid",cause);}
    char text[32];int n=snprintf(text,sizeof(text),"%ld\n",(long)getpid());
    int ok=!ftruncate(pid,0) && write(pid,text,(size_t)n)==n;
    int cause=errno;close(pid);if(!ok) {close(dir);return failed("audio-write-pid",cause);}
    int log=regular(dir,"x2d-shutter-audio.log",O_WRONLY|O_CREAT,0600);close(dir);
    if(log<0) return failed("audio-open-log",errno);
    ok=!ftruncate(log,0) && redirect(log);close_log(log);
    if(!ok) return failed("audio-redirect",errno);
    /* 原厂动态 sleep 加载预览库；静态 launcher 自身不加载该库。exec 保持 PID。 */
    char *args[]={"/system/bin/sleep","2147483647",NULL};
    if(setenv("PATH","/system/bin:/system/xbin:/sbin",1) || setenv("X2D_AUDIO_SERVICE","1",1) ||
       setenv("LD_PRELOAD","/system/lib64/libx2d_audio_preview.so",1)) return failed("audio-environment",errno);
    if(interrupted) return 143;
    execve(args[0],args,environ);failed("audio-exec",errno);return 127;
}
int main(int argc,char **argv) {
    struct sigaction action;memset(&action,0,sizeof(action));action.sa_handler=on_signal;
    sigemptyset(&action.sa_mask);
    if(sigaction(SIGHUP,&action,NULL) || sigaction(SIGINT,&action,NULL) || sigaction(SIGTERM,&action,NULL)) return 1;
#ifdef CN_FULL_FLOW
    if(argc>1 && !strcmp(argv[1],"counter-sync")) return cn_counter_main(argc,argv);
    if(argc>1 && !strncmp(argv[1],"publish-",8)) return cn_publish_main(argc,argv);
    if(argc>1 && !strncmp(argv[1],"render-",7)) return cn_render_main(argc,argv);
    if(argc>1 && !strcmp(argv[1],"mode-prepare")) return cn_prepare_main(argc,argv);
    if(argc>1 && !strcmp(argv[1],"heif-restart")) return cn_heif_restart_main(argc,argv);
    if(argc>1 && !strcmp(argv[1],"ui-ready")) return cn_ui_check_main(argc,argv);
#ifndef CN_RUNTIME_ONLY
    if(argc>1 && (!strcmp(argv[1],"stage-init") || !strcmp(argv[1],"retained-cleanup"))) return cn_setup_main(argc,argv);
    if(argc>1 && !strcmp(argv[1],"uninstall")) return cn_uninstall_main(argc,argv);
    if(argc>1 && !strcmp(argv[1],"install-stock")) return cn_install_main(argc,argv);
#endif
    if(argc>1 && !strcmp(argv[1],"data-cleanup")) return cn_data_cleanup_main(argc,argv);
    if(argc>1 && (!strcmp(argv[1],"memory-run")||!strcmp(argv[1],"memory-restore")||!strcmp(argv[1],"memory-watchdog"))) return cn_memory_runner_main(argc,argv);
    if(argc>1 && (!strcmp(argv[1],"sixteen-run")||!strcmp(argv[1],"sixteen-restore")||!strcmp(argv[1],"sixteen-watchdog")||!strcmp(argv[1],"quarter-run")||!strcmp(argv[1],"quarter-restore")||!strcmp(argv[1],"quarter-watchdog"))) return cn_disk_runner_main(argc,argv);
    if(argc>1 && (!strcmp(argv[1],"launch-memory")||!strcmp(argv[1],"launch-multi")||!strcmp(argv[1],"launch-stack"))) return cn_launch_main(argc,argv);
    if(argc>1 && (!strcmp(argv[1],"stack-run")||!strcmp(argv[1],"stack-restore")||!strcmp(argv[1],"stack-watchdog"))) return cn_stack_runner_main(argc,argv);
#ifndef CN_RUNTIME_ONLY
    if(argc>1 && !strcmp(argv[1],"dispatch")) return cn_dispatch_main(argc,argv);
    if(argc>1 && !strcmp(argv[1],"transaction-read")) return cn_transaction_read_main(argc,argv);
#ifdef CN_PACKAGED_PAYLOAD
    if(argc>1 && !strcmp(argv[1],"payload-unpack")) return cn_unpack_main(argc,argv);
    if(argc>1 && (!strcmp(argv[1],"bootstrap-commit") || !strcmp(argv[1],"bootstrap-read") || !strcmp(argv[1],"bootstrap-cleanup"))) return cn_bootstrap_main(argc,argv);
#endif
#endif
#endif
    struct context c={.job=-1};
    const struct cn_ops ops={&c,prepare,memory,delay,cancelled,encode,verify,audio,event};
    struct cn_result result=cn_run(argc,(const char *const *)argv,&ops);
    if(c.job>=0) close(c.job);
    static const char *const phases[]={"arguments","validate-input","wait-memory","encode","verify-output","complete"};
    fprintf(stderr,"NATIVE_WRAPPER_FINISH phase=%s rc=%d memory_reads=%u\n",phases[result.phase],result.rc,result.memory_reads);
    return result.rc;
}
