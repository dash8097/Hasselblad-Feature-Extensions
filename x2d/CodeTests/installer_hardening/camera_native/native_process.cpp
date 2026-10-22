#include "native_process.hpp"
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <sstream>
#include <algorithm>
extern char **environ;
namespace cn {
static bool program(const std::string&p){
    for(const auto*q:{"/system/bin/x2d-pixel-overclock/render-memory","/system/bin/x2d-memory-manual-r17/render-probe",
        "/system/bin/x2d-multi-resolution/merge16","/system/bin/x2d-multi-resolution/render16",
        "/system/bin/x2d-multi-resolution/jpeg-grid","/system/bin/x2d-multi-resolution/pack16",
        "/system/bin/x2d-pixel-overclock/preview","/system/bin/x2d-pixel-overclock/inputs","/system/bin/x2d-pixel-overclock/heif"})if(p==q)return true;
    return false;
}
static int status_rc(int status){return WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?128+WTERMSIG(status):1;}
Child::~Child(){
    if(drain_on_destroy_&&pid_>0&&!done_){int s;pid_t n;do{n=waitpid(pid_,&s,0);}while(n<0&&errno==EINTR);}
}
void Child::start(const std::vector<std::string>&args,const std::string&log){
    cancelled();require(pid_==-1&&!args.empty()&&program(args[0]),"child-program",2);
    cancellable_utility_=args[0]=="/system/bin/x2d-pixel-overclock/inputs";
    independent_heif_=args[0]=="/system/bin/x2d-pixel-overclock/heif";
    require(args.size()<=16,"child-argc",2);
    std::vector<char*> av;av.reserve(args.size()+1);
    for(const auto&s:args){require(s.size()<2048&&s.find('\0')==std::string::npos,"child-argument",2);av.push_back(const_cast<char*>(s.c_str()));}av.push_back(nullptr);
    int output=open(log.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);require(output>=0,"child-log");
    pid_=fork();int cause=errno;
    if(pid_<0){close(output);errno=cause;throw Error("child-fork");}
    if(!pid_){
        signal(SIGHUP,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);
        int copy=fcntl(output,F_DUPFD_CLOEXEC,3);
        if(copy<0||dup2(copy,1)<0||dup2(copy,2)<0)_exit(126);
        close(copy);if(output!=1&&output!=2)close(output);
        execve(av[0],av.data(),environ);_exit(127);
    }
    close(output);
}
bool Child::poll(){
    require(pid_>0,"child-not-started");if(done_)return true;
    int s;pid_t n;do{n=waitpid(pid_,&s,WNOHANG);}while(n<0&&errno==EINTR);
    require(n>=0,"child-poll");if(n==pid_){done_=true;rc_=status_rc(s);}return done_;
}
int Child::wait(bool interruptible){
    require(pid_>0,"child-not-started");if(done_)return rc_;
    if(interruptible){while(!poll())sleep_ms(20);cancelled();return rc_;}
    int s;pid_t n;do{n=waitpid(pid_,&s,0);}while(n<0&&errno==EINTR);
    require(n==pid_,"child-wait");done_=true;rc_=status_rc(s);return rc_;
}
int Child::wait_utility(){
    require(cancellable_utility_,"child-not-cancellable-utility",2);
    try{return wait(true);}catch(...){
        Recovery recovery;
        if(!poll()){
            require(!kill(pid_,SIGTERM)||errno==ESRCH,"utility-term",90);
            unsigned attempts=0;while(!poll()&&attempts++<50)sleep_ms(20);
            if(!done_){require(!kill(pid_,SIGKILL)||errno==ESRCH,"utility-kill",90);wait();}
        }
        throw;
    }
}
void Child::terminate_after_release(const std::string&expected){
    require(pid_>0,"child-not-started");if(poll())return;
    require(executable(pid_)==expected,"child-identity-before-terminate",90);
    require(!kill(pid_,SIGTERM)||errno==ESRCH,"child-term",90);wait();
}
void Child::terminate_heif_bounded(){
    // 只用于原厂服务已退出后的独立 HEIF 编码器。不能套用于持有 RAW/DMA 引用的显影。
    require(independent_heif_&&!drain_on_destroy_&&pid_>0,"heif-child-contract",90);
    if(poll())return;
    require(executable(pid_)=="/system/bin/x2d-pixel-overclock/heif","heif-child-identity",90);
    require(!kill(pid_,SIGTERM)||errno==ESRCH,"heif-child-term",90);
    auto deadline=monotonic_ms()+2000;
    while(!poll()&&monotonic_ms()<deadline)sleep_ms(20);
    if(done_)return;
    // 尚未 waitpid 回收的直属子进程不会发生 PID 复用。重新核对 exec 身份。
    require(executable(pid_)=="/system/bin/x2d-pixel-overclock/heif","heif-child-identity-before-kill",90);
    require(!kill(pid_,SIGKILL)||errno==ESRCH,"heif-child-kill",90);
    deadline=monotonic_ms()+2000;
    while(!poll()&&monotonic_ms()<deadline)sleep_ms(20);
    require(done_,"heif-child-exit-unconfirmed",90);
}
unsigned log_count(const std::string&path,const std::string&line){
    if(!exists(path))return 0;auto text=read(path,16*1024*1024);std::istringstream in(text);std::string item;unsigned count=0;
    while(std::getline(in,item))if(item==line)count++;return count;
}
unsigned saved_count(const std::string&path,const std::string&prefix){
    auto text=read(path,16*1024*1024);std::istringstream in(text);std::string item;unsigned count=0;
    while(std::getline(in,item)){if(item.rfind(prefix,0))continue;auto n=item.substr(prefix.size());if(!n.empty()&&n[0]>='1'&&n[0]<='9'&&digits(n))count++;}return count;
}
bool log_prefix(const std::string&path,const std::string&prefix){
    auto text=read(path,16*1024*1024);std::istringstream in(text);std::string item;
    while(std::getline(in,item))if(item.rfind(prefix,0)==0)return true;return false;
}
void lease_remove(const std::string&path,pid_t owner){
    if(!exists(path))return;auto data=read(path,65536);require(data.size()>=12,"lease-size",90);
    auto*b=reinterpret_cast<const unsigned char*>(data.data());uint32_t id=uint32_t(b[8])|uint32_t(b[9])<<8|uint32_t(b[10])<<16|uint32_t(b[11])<<24;
    require(owner>0&&id==uint32_t(owner),"lease-owner",90);remove_regular(path);
}
}
