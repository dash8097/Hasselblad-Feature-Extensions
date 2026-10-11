/* Reuse the existing persistent disable marker; no second settings database.
 * Missing marker=both; legacy empty marker=animation only; "0\n"=off.
 * Root paths are fixed in production; tests use a temporary directory. */
#ifndef EFFECT_MODE_H
#define EFFECT_MODE_H
#ifndef EFFECT_MODE_TEST
/* Fixed AArch64 Linux/bionic ABI; keep the freestanding firmware build. */
typedef unsigned long size_t;
typedef long ssize_t;
extern int open(const char *,int,...),close(int),fsync(int),unlink(const char *);
extern int rename(const char *,const char *);
extern ssize_t read(int,void *,size_t),write(int,const void *,size_t);
extern int *__errno(void);
#define errno (*__errno())
#define ENOENT 2
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 0100
#define O_EXCL 0200
#define O_NONBLOCK 04000
#define O_DIRECTORY 040000
#define O_NOFOLLOW 0100000
#endif
#ifndef EFFECT_DIRECTORY
#define EFFECT_DIRECTORY "/blackbox"
#endif
#define EFFECT_MARKER EFFECT_DIRECTORY "/x2d-shutter-audio.disable"
#define EFFECT_PENDING EFFECT_DIRECTORY "/x2d-shutter-audio.disable.next"
static int effect_mode_load(void) {
    int fd=open(EFFECT_MARKER,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return errno==ENOENT ? 2 : -1;
    char value[3]; ssize_t n=read(fd,value,sizeof(value)); close(fd);
    if(n==0)return 1; /* Old sound-only disable marker. */
    if(n==2 && value[1]=='\n' && (value[0]=='0'||value[0]=='1'))return value[0]-'0';
    return -1; /* Unknown content: do not silently enable effects. */
}
static int effect_mode_save(int mode) {
    if(mode<0||mode>2)return -1;
    if(mode==2){
        /* Verify readable regular content before removing a marker. */
        if(effect_mode_load()<0)return -1;
        if(unlink(EFFECT_MARKER)<0 && errno!=ENOENT)return -1;
    }else{
        if(effect_mode_load()<0)return -1;
        int fd=open(EFFECT_PENDING,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
        if(fd<0)return -1;
        char value[2]={(char)('0'+mode),'\n'};
        int ok=write(fd,value,2)==2 && fsync(fd)==0;
        if(close(fd)!=0)ok=0;
        if(!ok){unlink(EFFECT_PENDING);return -1;}
        if(rename(EFFECT_PENDING,EFFECT_MARKER)!=0){unlink(EFFECT_PENDING);return -1;}
    }
    int dir=open(EFFECT_DIRECTORY,O_RDONLY|O_DIRECTORY);
    if(dir<0)return -1;
    int ok=fsync(dir)==0;close(dir);
    return ok ? 0 : -1;
}
#endif
