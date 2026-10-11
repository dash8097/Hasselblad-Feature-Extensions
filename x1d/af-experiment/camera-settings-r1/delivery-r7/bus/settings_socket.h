#ifndef AF_SETTINGS_SOCKET_H
#define AF_SETTINGS_SOCKET_H
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#ifndef AS_DIRECTORY
#define AS_DIRECTORY "/tmp/hbl-af-settings"
#endif
#define AS_BACKEND AS_DIRECTORY "/backend.sock"
#define AS_UI AS_DIRECTORY "/ui.sock"
// 固定相机 ARM32 glibc 2.22；交叉编译头的 struct stat 与目标布局不同。
#if defined(__arm__) && defined(__GLIBC__)
extern "C" int __lxstat64(int,const char *,void *);
#endif
static inline int as_stat(const char *path,uint32_t &mode,uint32_t &uid){
#if defined(__arm__) && defined(__GLIBC__)
    uint64_t aligned[13]={};
    static_assert(sizeof(aligned)==104,"ARM32 kernel stat64 ABI");
    if(__lxstat64(3,path,aligned))return -1;
    std::memcpy(&mode,reinterpret_cast<const char *>(aligned)+16,4);
    std::memcpy(&uid,reinterpret_cast<const char *>(aligned)+24,4);
#else
    struct stat s;if(lstat(path,&s))return -1;mode=s.st_mode;uid=s.st_uid;
#endif
    return 0;
}
static inline bool as_directory(){uint32_t mode=0,uid=0;return !as_stat(AS_DIRECTORY,mode,uid) && S_ISDIR(mode) && uid==geteuid() && (mode&0777)==0700;}
static inline int as_bind(const char *path){
    if(!as_directory())return -1;
    int fd=socket(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(fd<0)return -1;
    int one=1;sockaddr_un a={};a.sun_family=AF_UNIX;std::strcpy(a.sun_path,path);
    if(setsockopt(fd,SOL_SOCKET,SO_PASSCRED,&one,sizeof(one)) || bind(fd,reinterpret_cast<sockaddr *>(&a),offsetof(sockaddr_un,sun_path)+std::strlen(path)+1)){close(fd);return -1;}
    if(chmod(path,0600)){close(fd);unlink(path);return -1;}return fd;
}
static inline bool as_send(int fd,const char *path,const unsigned char *p){
    uint32_t mode=0,uid=0;if(fd<0 || !as_directory() || as_stat(path,mode,uid) || !S_ISSOCK(mode) || uid!=geteuid() || (mode&0777)!=0600)return false;
    sockaddr_un a={};a.sun_family=AF_UNIX;std::strcpy(a.sun_path,path);
    return sendto(fd,p,255,MSG_DONTWAIT|MSG_NOSIGNAL,reinterpret_cast<sockaddr *>(&a),sizeof(a))==255;
}
/* 只给独立 r3 bridge 记录最后拒绝类别，不记录 UID、路径或报文。 */
static inline int as_recv(int fd,unsigned char *p,const char *peer,unsigned *reason=nullptr){
    if(reason)*reason=0;
    sockaddr_un a={};iovec io={p,255};char ancillary[CMSG_SPACE(sizeof(ucred))]={};msghdr h={};
    h.msg_name=&a;h.msg_namelen=sizeof(a);h.msg_iov=&io;h.msg_iovlen=1;h.msg_control=ancillary;h.msg_controllen=sizeof(ancillary);
    ssize_t n=recvmsg(fd,&h,MSG_DONTWAIT);if(n<0)return -1;
    unsigned rejected=0;
    if(n!=255)rejected=1;
    else if(h.msg_flags&(MSG_TRUNC|MSG_CTRUNC))rejected=2;
    else if(a.sun_family!=AF_UNIX)rejected=3;
    else if(h.msg_namelen!=offsetof(sockaddr_un,sun_path)+std::strlen(peer)+1)rejected=4;
    else if(std::strcmp(a.sun_path,peer))rejected=5;
    if(rejected){if(reason)*reason=rejected;return 0;}
    for(cmsghdr *c=CMSG_FIRSTHDR(&h);c;c=CMSG_NXTHDR(&h,c))
        if(c->cmsg_level==SOL_SOCKET && c->cmsg_type==SCM_CREDENTIALS && c->cmsg_len==CMSG_LEN(sizeof(ucred))){
            ucred credentials;std::memcpy(&credentials,CMSG_DATA(c),sizeof(credentials));if(credentials.uid==geteuid())return 1;if(reason)*reason=7;return 0;
        }
    if(reason)*reason=6;return 0;
}
#endif
