#ifndef HBL_RF_LOCAL_SOCKET_H
#define HBL_RF_LOCAL_SOCKET_H
#include "rf_bridge.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stddef.h>

static const char RF_WORKER_SOCKET[]="/tmp/hbl-wireless-flash/worker.sock";
static const char RF_UI_SOCKET[]="/tmp/hbl-wireless-flash/ui.sock";
/* 固定相机 ARM32 glibc 2.22 ABI；现代交叉编译头的 stat 布局不同。 */
#if defined(__arm__) && defined(__GLIBC__)
#ifdef __cplusplus
extern "C"
#endif
int __lxstat64(int,const char *,void *);
#endif
static inline int rf_local_stat(const char *path,uint32_t *mode,uint32_t *uid) {
#if defined(__arm__) && defined(__GLIBC__)
    uint64_t aligned[13]={0};
    if (__lxstat64(3,path,aligned)) return -1;
    memcpy(mode,(const char *)aligned+16,4);
    memcpy(uid,(const char *)aligned+24,4);
#else
    struct stat st;
    if (lstat(path,&st)) return -1;
    *mode=st.st_mode; *uid=st.st_uid;
#endif
    return 0;
}
static inline uint64_t rf_monotonic_ms(void) {
    struct timespec ts; if (clock_gettime(CLOCK_MONOTONIC,&ts)) return 0;
    return (uint64_t)ts.tv_sec*1000u+(uint64_t)ts.tv_nsec/1000000u;
}
static inline int rf_local_address(struct sockaddr_un *a,const char *path) {
    const size_t n=strlen(path);
    if (n>=sizeof(a->sun_path)) return 0;
    memset(a,0,sizeof(*a)); a->sun_family=AF_UNIX; memcpy(a->sun_path,path,n+1);
    return (int)(offsetof(struct sockaddr_un,sun_path)+n+1);
}
static inline int rf_local_bind(const char *path) {
    struct sockaddr_un a; uint32_t mode=0,uid=0; int fd,len=rf_local_address(&a,path);
    if (!len) return -1;
    if (!rf_local_stat(path,&mode,&uid)) {
        if (!S_ISSOCK(mode) || uid!=geteuid() || unlink(path)) return -1;
    } else if (errno!=ENOENT) return -1;
    fd=socket(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
    if (fd<0) return -1;
    if (bind(fd,(struct sockaddr *)&a,(socklen_t)len) || chmod(path,0600)) { close(fd); return -1; }
    return fd;
}
static inline int rf_local_send(int fd,const char *path,const RfBridgePacket *packet) {
    struct sockaddr_un a; int len=rf_local_address(&a,path);
    if (fd<0 || !len) return 0;
    return sendto(fd,packet,sizeof(*packet),MSG_DONTWAIT|MSG_NOSIGNAL,(struct sockaddr *)&a,(socklen_t)len)==(ssize_t)sizeof(*packet);
}
/* 返回 1 是完整包，0 为无数据，-1 为无效包或读取失败。 */
static inline int rf_local_receive(int fd,RfBridgePacket *packet,char *peer,size_t peerSize) {
    struct sockaddr_un from; struct iovec iov={packet,sizeof(*packet)}; struct msghdr msg;
    ssize_t n; size_t nameBytes;
    memset(&from,0,sizeof(from)); memset(&msg,0,sizeof(msg));
    msg.msg_name=&from; msg.msg_namelen=sizeof(from); msg.msg_iov=&iov; msg.msg_iovlen=1;
    n=recvmsg(fd,&msg,MSG_DONTWAIT);
    if (n<0) return (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) ? 0 : -1;
    if ((msg.msg_flags&MSG_TRUNC) || !rf_bridge_valid(packet,(unsigned)n) ||
        from.sun_family!=AF_UNIX || msg.msg_namelen<=offsetof(struct sockaddr_un,sun_path)) return -1;
    nameBytes=msg.msg_namelen - offsetof(struct sockaddr_un,sun_path);
    if (nameBytes>sizeof(from.sun_path) || !memchr(from.sun_path,0,nameBytes)) return -1;
    nameBytes=strlen(from.sun_path);
    if (nameBytes+1>peerSize) return -1;
    memcpy(peer,from.sun_path,nameBytes+1); return 1;
}
#endif
