#ifndef HBL_FORMAL_INSTALL_HOLD_H
#define HBL_FORMAL_INSTALL_HOLD_H
#include "rf_local_socket.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

// 只访问本轮 root 私有目录；截止时间由一次性的安装事务创建。
#ifndef HBL_HOLD_STATE_DIR
#define HBL_HOLD_STATE_DIR "/tmp/hbl-wireless-flash/formal-state"
#endif
static const char * const holdStateDirectory=HBL_HOLD_STATE_DIR;
static const char * const holdDeadlinePath=HBL_HOLD_STATE_DIR "/hold.deadline";
static const char * const holdReleasePath=HBL_HOLD_STATE_DIR "/hold.release";
static const char * const holdPulsePath=HBL_HOLD_STATE_DIR "/hold.pulse";
static const uint64_t holdMaximumMs=1200000;
#if defined(__arm__) && defined(__GLIBC__)
extern "C" int __fxstat64(int,int,void *);
#endif
static inline bool holdDirectorySafe(const char *path) {
    uint32_t mode=0,uid=0;
    return !rf_local_stat(path,&mode,&uid) && S_ISDIR(mode) && uid==0 && (mode&0777)==0700;
}
static inline bool holdFileMetadata(int fd,uint32_t &mode,uint32_t &uid,uint32_t &links,int64_t &size) {
#if defined(__arm__) && defined(__GLIBC__)
    // 与 rf_local_stat 相同的 ARM32 glibc 2.22 / kernel stat64 104 字节 ABI。
    uint64_t bytes[13]={};
    if(__fxstat64(3,fd,bytes)) return false;
    memcpy(&mode,reinterpret_cast<char *>(bytes)+16,4);
    memcpy(&links,reinterpret_cast<char *>(bytes)+20,4);
    memcpy(&uid,reinterpret_cast<char *>(bytes)+24,4);
    memcpy(&size,reinterpret_cast<char *>(bytes)+48,8);
#else
    struct stat s;if(fstat(fd,&s)) return false;
    mode=s.st_mode;uid=s.st_uid;links=s.st_nlink;size=s.st_size;
#endif
    return true;
}
static inline uint64_t holdNow() {
    timespec t; if(clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return uint64_t(t.tv_sec)*1000+uint64_t(t.tv_nsec)/1000000;
}
static inline bool holdWindow(uint64_t now,uint64_t deadline) {
    return now && deadline>now && deadline-now<=holdMaximumMs;
}
static inline bool holdFresh(uint64_t now,uint64_t deadline,uint64_t pulse,unsigned pid,unsigned actualPid) {
    return holdWindow(now,deadline) && pid && pid==actualPid && pulse && pulse<=now && now-pulse<=2000;
}
static inline bool holdRead(const char *path,char *text,size_t size) {
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC); if(fd<0) return false;
    uint32_t mode=0,uid=0,links=0;int64_t fileSize=0;
    bool ok=holdFileMetadata(fd,mode,uid,links,fileSize) && S_ISREG(mode) && uid==0 && (mode&0777)==0600 && links==1 && fileSize>0 && uint64_t(fileSize)<size;
    ssize_t n=ok ? read(fd,text,size-1) : -1; close(fd);
    if(n<=0 || n!=fileSize) return false;
    text[n]=0; return true;
}
static inline bool holdReleased() {
    uint32_t mode=0,uid=0;
    return !rf_local_stat(holdReleasePath,&mode,&uid) || errno!=ENOENT;
}
static inline uint64_t holdReadDeadline() {
    char b[96],extra; unsigned long long deadline=0;
    if(!holdRead(holdDeadlinePath,b,sizeof(b)) || sscanf(b,"HHD1 %llu %c",&deadline,&extra)!=1) return 0;
    return uint64_t(deadline);
}
static inline bool holdAtomicPulse(unsigned pid,uint64_t deadline,uint64_t pulse) {
    const char *path=HBL_HOLD_STATE_DIR "/hold.pulse.next";
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600); if(fd<0) return false;
    char b[120]; int n=snprintf(b,sizeof(b),"HPI1 %u %llu %llu\n",pid,(unsigned long long)deadline,(unsigned long long)pulse);
    bool ok=write(fd,b,size_t(n))==n; if(close(fd)) ok=false;
    if(ok) ok=!rename(path,holdPulsePath);
    if(!ok) unlink(path);
    return ok;
}
#endif
