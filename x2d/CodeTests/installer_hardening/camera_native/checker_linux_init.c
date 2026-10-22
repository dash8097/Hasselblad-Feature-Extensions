/* 独立 ARM64 init：真实 fork/exec/openat/symlink/mount/signal/wait 的黑盒测试。 */
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
static unsigned checks,cases;
static char job[256];
static void fail(int line,const char*s){printf("CHECK_FAILED line=%d expr=%s errno=%d\n",line,s,errno);fflush(stdout);reboot(RB_POWER_OFF);for(;;)pause();}
#define C(x) do{checks++;if(!(x))fail(__LINE__,#x);}while(0)
static void writefile(const char*p,const char*s){int f=open(p,O_WRONLY|O_CREAT|O_TRUNC,0600);C(f>=0);C(write(f,s,strlen(s))==(ssize_t)strlen(s));C(!close(f));}
static void path(char*out,const char*n){snprintf(out,512,"%s/%s",job,n);}
static void file(const char*n,const char*s){char p[512];path(p,n);writefile(p,s);}
static int exists(const char*n){char p[512];path(p,n);return access(p,F_OK)==0;}
static void removefile(const char*n){char p[512];path(p,n);C(!unlink(p));}
static void linkfile(const char*n,const char*target){char p[512];path(p,n);C(!symlink(target,p));}
static void reset(void){
 cases++;snprintf(job,sizeof(job),"/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-%u",cases);C(!mkdir(job,0700));
 file("native.disk.ready","NATIVE_DISK_RAW_JPEG_DURABLE\n");file("album.jpg","JPEG_REFERENCE");writefile("/tmp/encoder-mode","ok");
}
static pid_t start(const char*command,const char*j){pid_t p=fork();C(p>=0);if(!p){char*args[]={"/flow",(char*)command,(char*)j,NULL};if(!j)args[2]=NULL;execv(args[0],args);_exit(127);}return p;}
static int result(pid_t p){int s;while(waitpid(p,&s,0)<0){if(errno!=EINTR)fail(__LINE__,"waitpid");}return WIFEXITED(s)?WEXITSTATUS(s):128+WTERMSIG(s);}
static void expect(const char*name,int rc,int spawned){int actual=result(start("heif-disk",job));printf("CASE %s rc=%d expected=%d\n",name,actual,rc);C(actual==rc);C(exists("encoder.started")==spawned);C(exists("album.jpg"));}
static void memory(const char*s){writefile("/tmp/meminfo",s);C(!mount("/tmp/meminfo","/proc/meminfo",NULL,MS_BIND,NULL));}
static void memory_end(void){C(!umount("/proc/meminfo"));}
int main(void){
 mount("devtmpfs","/dev","devtmpfs",0,NULL);
 if(access("/dev/console",F_OK))mknod("/dev/console",S_IFCHR|0600,makedev(5,1));
 int console=open("/dev/console",O_RDWR);if(console>=0){dup2(console,0);dup2(console,1);dup2(console,2);if(console>2)close(console);}
 setbuf(stdout,NULL);setbuf(stderr,NULL);
 C(getpid()==1);C(!mount("proc","/proc","proc",0,NULL));C(!mount("tmpfs","/tmp","tmpfs",0,"mode=1777"));
 printf("LINUX_POSIX_BEGIN real_kernel=1 arm64=1 camera=0\n");
 memory("MemTotal: 1048576 kB\nMemAvailable: 600000 kB\n");
 reset();expect("success",0,1);C(exists("encoder.finished"));
 reset();file("album.heic","OLD");expect("existing-heic",1,0);
 reset();file("heif.ready","OLD");expect("existing-ready",1,0);
 reset();linkfile("album.heic","/does-not-exist");expect("dangling-heic",1,0);
 reset();linkfile("heif.ready","/does-not-exist");expect("dangling-ready",1,0);
 reset();removefile("album.jpg");linkfile("album.jpg","/tmp/source");writefile("/tmp/source","SOURCE");expect("jpeg-symlink",1,0);
 reset();removefile("native.disk.ready");linkfile("native.disk.ready","/tmp/marker");writefile("/tmp/marker","NATIVE_DISK_RAW_JPEG_DURABLE\n");expect("marker-symlink-hardened",1,0);
 reset();file("album.jpg","");expect("empty-jpeg",1,0);
 reset();file("native.disk.ready","BAD\n");expect("wrong-marker",1,0);
 reset();linkfile("heif.log","/mnt/media_rw/ssd/protected");writefile("/mnt/media_rw/ssd/protected","PROTECTED");expect("log-symlink",1,0);
 reset();char p[512];path(p,"heif.log");C(!link("/mnt/media_rw/ssd/protected",p));expect("log-hardlink",1,0);
 reset();path(p,"heif.log");C(!mkfifo(p,0600));expect("log-fifo",1,0);
 reset();path(p,"heif.log");C(!mkdir(p,0700));expect("log-directory",1,0);
 reset();writefile("/tmp/encoder-mode","exit17");expect("encoder-exit17",17,1);
 reset();writefile("/tmp/encoder-mode","error");expect("x265-error-now-rejected",1,1);
 reset();writefile("/tmp/encoder-mode","bad");expect("wrong-ready",1,1);
 reset();writefile("/tmp/encoder-mode","missing");expect("missing-ready",1,1);
 reset();C(!rename("/mnt/media_rw/ssd/multishot","/mnt/media_rw/ssd/multishot-real"));C(!symlink("multishot-real","/mnt/media_rw/ssd/multishot"));expect("parent-symlink",1,0);C(!unlink("/mnt/media_rw/ssd/multishot"));C(!rename("/mnt/media_rw/ssd/multishot-real","/mnt/media_rw/ssd/multishot"));
 memory_end();memory("MemAvailable: invalid kB\n");reset();expect("invalid-meminfo",19,0);memory_end();
 memory("MemAvailable: 600000 kB\nMemAvailable: 600000 kB\n");reset();expect("duplicate-meminfo",19,0);memory_end();
 memory("MemAvailable: 573440 kB\n");reset();expect("exact-memory-threshold",0,1);memory_end();
 memory("MemAvailable: 0 kB\n");reset();expect("real-49-sleeps-exhausted",19,0);memory_end();
 memory("MemAvailable: 0 kB\n");reset();pid_t slow=start("heif-disk",job);usleep(350000);C(!kill(slow,SIGTERM));C(result(slow)==143);C(!exists("encoder.started"));puts("CASE signal-during-memory rc=143");memory_end();
 memory("MemAvailable: 600000 kB\n");reset();writefile("/tmp/encoder-mode","wait");slow=start("heif-disk",job);
 unsigned n=0;while(!exists("encoder.started")&&n++<200)usleep(10000);C(exists("encoder.started"));C(!kill(slow,SIGTERM));C(result(slow)==143);C(exists("encoder.finished"));puts("CASE signal-waits-owned-child rc=143 child_finished=1");memory_end();
 cases++;C(result(start("audio",NULL))==0);C(!access("/tmp/audio-result",F_OK));puts("CASE audio-exec-pid-env rc=0");
 /* 确认危险文件未被日志重定向截断。 */
 char protected[32]={0};int fd=open("/mnt/media_rw/ssd/protected",O_RDONLY);C(fd>=0);C(read(fd,protected,sizeof(protected))==9);close(fd);C(!strcmp(protected,"PROTECTED"));
 printf("LINUX_POSIX_ALL_PASSED cases=%u checks=%u\n",cases,checks);sync();reboot(RB_POWER_OFF);return 0;
}
