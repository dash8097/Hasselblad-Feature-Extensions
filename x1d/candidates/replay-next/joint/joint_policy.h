#ifndef X1D_REPLAY_JOINT_POLICY_H
#define X1D_REPLAY_JOINT_POLICY_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include "../../../combined-runtime/native/install_window.h"

// 共同 install-state 仅由协调方写；本模块只拥有 replay-state。
static const char *const sessionRoot="/tmp/hbl-x1d-combined";
static const char *const sessionState="/tmp/hbl-x1d-combined/replay-state";
static const char *const gpuPath="/tmp/hbl-x1d-combined/replay-state/gpu";
static inline uint64_t nowMs() {
    timespec t; if(clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return uint64_t(t.tv_sec)*1000+uint64_t(t.tv_nsec)/1000000;
}
struct FileState { uint32_t mode=0,uid=0,nlink=0; int64_t size=0; };
#if defined(__arm__) && defined(__linux__)
extern "C" int __lxstat64(int,const char *,void *);
extern "C" int __fxstat64(int,int,void *);
static inline bool fileState(const char *path,int fd,FileState &s) {
    // 固定 ARM glibc 2.22 / kernel stat64，避免现代工具链 stat 结构/版本差异。
    alignas(8) unsigned char b[104]={};
    if(path ? __lxstat64(3,path,b) : __fxstat64(3,fd,b)) return false;
    memcpy(&s.mode,b+16,4); memcpy(&s.nlink,b+20,4); memcpy(&s.uid,b+24,4); memcpy(&s.size,b+48,8);
    return true;
}
#else
static inline bool fileState(const char *path,int fd,FileState &s) {
    struct stat st; if(path ? lstat(path,&st) : fstat(fd,&st)) return false;
    s.mode=st.st_mode; s.uid=st.st_uid; s.nlink=st.st_nlink; s.size=st.st_size; return true;
}
#endif
static inline bool privateDirectory(const char *path) {
    FileState s; return fileState(path,-1,s) && S_ISDIR(s.mode) && s.uid==0 && (s.mode&0777)==0700;
}
static inline bool privateRead(const char *path,char *text,size_t size) {
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC); if(fd<0) return false;
    FileState s;
    bool ok=fileState(nullptr,fd,s) && S_ISREG(s.mode) && s.uid==0 && (s.mode&0777)==0600 && s.nlink==1 && s.size>0 && uint64_t(s.size)<size;
    ssize_t n=ok ? read(fd,text,size-1) : -1; close(fd);
    if(n<=0 || n!=s.size) return false;
    text[n]=0; return true;
}
static inline bool jointWindowActive() {
    return privateDirectory(sessionRoot) && privateDirectory("/tmp/hbl-x1d-combined/install-state") && hbl_combined_window_active();
}
static inline bool atomicText(const char *path,const char *text) {
    if(!privateDirectory(sessionRoot) || !privateDirectory(sessionState)) return false;
    char temporary[192]; int count=snprintf(temporary,sizeof(temporary),"%s.next",path);
    if(count<=0 || size_t(count)>=sizeof(temporary)) return false;
    int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600); if(fd<0) return false;
    size_t n=strlen(text); bool ok=write(fd,text,n)==ssize_t(n);
    if(close(fd)) ok=false;
    if(ok) ok=!rename(temporary,path);
    if(!ok) unlink(temporary);
    return ok;
}
static inline bool gpuAllowed(int maxSize,bool bgra,bool npot) {
    // 要求 NPOT，避免后续 mipmap 选项触发 8192² 重采样。
    return maxSize>=8176 && bgra && npot;
}
#endif
