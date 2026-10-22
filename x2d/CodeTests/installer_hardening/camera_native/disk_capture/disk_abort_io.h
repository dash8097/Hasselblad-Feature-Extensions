/* 固定取消协议：仅在曝光/RPC/存储均 idle 后由 runner 写入。
 * QUARTER token 额外证明 renderer 从未启动，或已按 PID/start 身份排空退出。
 * 请求不是拍摄完成回执，不修改原厂通知结果，也不移除未完成源文件。 */
#include <sys/stat.h>
#include <errno.h>
static int da_read(const char *dir,const char *name,const char *expected){
 char path[256],data[128];int n=snprintf(path,sizeof path,"%s/%s",dir,name);
 if(n<=0||(size_t)n>=sizeof path)return -1;
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return errno==ENOENT?0:-1;
 struct stat st;int ok=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_size==(off_t)strlen(expected);
 ssize_t bytes=ok?read(fd,data,sizeof data):-1;if(close(fd))ok=0;
 return ok&&bytes==(ssize_t)strlen(expected)&&!memcmp(data,expected,(size_t)bytes)?1:-1;
}
static int da_receipt(const char *dir,const char *token){
 char path[256];int n=snprintf(path,sizeof path,"%s/abort.drained",dir);
 if(n<=0||(size_t)n>=sizeof path)return 0;
 int existing=da_read(dir,"abort.drained",token)==1;
 int fd=open(path,O_WRONLY|O_NOFOLLOW|(existing?0:(O_CREAT|O_EXCL)),0600);if(fd<0)return 0;
 struct stat st;int ok=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1;
 if(ok&&!existing)ok=write(fd,token,strlen(token))==(ssize_t)strlen(token);
 if(fsync(fd))ok=0;if(close(fd))ok=0;
 int d=open(dir,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(d<0)return 0;
 if(fsync(d))ok=0;if(close(d))ok=0;return ok;
}
