/* 固定系统 API：不会解析或执行 shell/awk/sed 文本。 */
#include "native_common.hpp"
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <algorithm>
#include "receipt_sha256.h"
extern "C" int cn_cancel_requested(void);
extern "C" void cn_recovery_mode(int);
extern "C" int cn_job_valid(const char *);
extern char **environ;
namespace cn {
int system_remount(unsigned long flags){
    if(!mount(nullptr,"/system",nullptr,flags,nullptr))return 0;
    const int original=errno;
    if(original!=EACCES&&original!=EPERM)return -1;
    // 原厂服务环境允许固定 OS mount 工具；不修改 SELinux、凭据或文件标签。
    // 仅处理明确拒绝的 syscall，不重试超时或状态不明的写入。
    const char*tool="/system/bin/toybox";
    struct stat st;
    if(lstat(tool,&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||(st.st_mode&0022)) {errno=original;return -1;}
    if(sha256(tool)!="999a0669ef654efbda54bc585c0f3c00643d1af61c985bfb7968cfd2f9aed840"){errno=original;return -1;}
    require((flags&MS_REMOUNT)!=0,"system-remount-flags",90);
    auto result=command({tool,"mount",(flags&MS_RDONLY)?"-oremount,ro":"-oremount,rw","/system"},6000,false);
    if(result.rc){errno=original;return -1;}
    return 0; // 调用者仍必须独立核对挂载状态。
}
struct FD { int n; explicit FD(int v=-1):n(v){} ~FD(){if(n>=0)close(n);} };
void require(bool v,const std::string&s,int rc){if(!v)throw Error(s,rc);}
void require(bool v,const char*s,int rc){if(!v)throw Error(s,rc);}
Recovery::Recovery(){cn_recovery_mode(1);}
Recovery::~Recovery(){cn_recovery_mode(0);}
TransactionGuard::TransactionGuard(){
    FD parent(open("/tmp",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));require(parent.n>=0,"transaction-lock-parent",72);
    FD lock(openat(parent.n,"x2dt",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600));
    if(lock.n<0)throw Error("transaction-lock-open",72,false,errno);
    if(lock.n<3){int copy=fcntl(lock.n,F_DUPFD_CLOEXEC,3);if(copy<0)throw Error("transaction-lock-descriptor",72,false,errno);close(lock.n);lock.n=copy;}
    struct stat st{},now{};require(!fstat(lock.n,&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_uid==geteuid()&&(st.st_mode&0777)==0600,"transaction-lock-type",72);
    if(flock(lock.n,LOCK_EX|LOCK_NB))throw Error("transaction-already-active",72,false,errno);
    require(!fstatat(parent.n,"x2dt",&now,AT_SYMLINK_NOFOLLOW)&&now.st_dev==st.st_dev&&now.st_ino==st.st_ino,"transaction-lock-replaced",72);
    // 不 unlink：等待者必须始终竞争同一 inode。异常或进程退出由内核释放 flock。
    fd=lock.n;lock.n=-1;
}
TransactionGuard::~TransactionGuard(){if(fd>=0)close(fd);}
bool stopping(){return cn_cancel_requested()!=0;}
void cancelled(){if(stopping())throw Error("cancelled",143);}
void cancel_before_capture(const std::string&job){if(!exists(job+"/cancel.request"))return;require(read(job+"/cancel.request",16)=="CANCEL\n","capture-cancel-record");write_new(job+"/cancel.accepted","CANCELLED_BEFORE_CAPTURE_V1\n");throw Error("user-cancelled-before-capture",71);}
void sleep_ms(unsigned ms){
    cancelled();timespec rest={static_cast<time_t>(ms/1000),static_cast<long>((ms%1000)*1000000)};
    while(nanosleep(&rest,&rest)){require(errno==EINTR,"nanosleep");cancelled();}
    cancelled();
}
uint64_t monotonic_ms(){timespec t;require(!clock_gettime(CLOCK_MONOTONIC,&t),"monotonic-clock");return uint64_t(t.tv_sec)*1000+uint64_t(t.tv_nsec)/1000000;}
bool exists(const std::string&p){struct stat st;if(!lstat(p.c_str(),&st))return true;require(errno==ENOENT,"path-state");return false;}
bool regular(const std::string&p,bool nonempty){struct stat st;return !lstat(p.c_str(),&st)&&S_ISREG(st.st_mode)&&(!nonempty||st.st_size>0);}
bool directory(const std::string&p){struct stat st;return !lstat(p.c_str(),&st)&&S_ISDIR(st.st_mode);}
void job(const std::string&p){
    require(cn_job_valid(p.c_str()),"job-name",2);
    FD parent(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));require(parent.n>=0,"job-root");
    std::istringstream parts(p.substr(1));std::string part;
    while(std::getline(parts,part,'/')){
        require(!part.empty()&&part!="."&&part!="..","job-component",2);
        int next=openat(parent.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(next<0)throw Error("job-directory",1,false,errno);
        close(parent.n);parent.n=next;
    }
}
std::string read(const std::string&p,size_t limit){
    FD fd(open(p.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));require(fd.n>=0,"read-open:"+p);
    struct stat st;require(!fstat(fd.n,&st)&&S_ISREG(st.st_mode),"read-type:"+p);
    std::string out;char b[4096];
    for(;;){ssize_t n=::read(fd.n,b,sizeof b);if(n<0&&errno==EINTR)continue;require(n>=0,"read-data:"+p);if(!n)break;
        require(out.size()+size_t(n)<=limit,"read-limit:"+p);out.append(b,size_t(n));}
    return out;
}
std::string trim_newlines(std::string s){while(!s.empty()&&s.back()=='\n')s.pop_back();return s;}
std::string value(const std::string&p,size_t limit){auto s=read(p,limit);require(s.find('\0')==std::string::npos,"value-nul:"+p);return trim_newlines(s);}
static void write_fd(int fd,const std::string&text,bool flush=true){size_t n=0;while(n<text.size()){ssize_t k=::write(fd,text.data()+n,text.size()-n);if(k<0&&errno==EINTR)continue;if(k<=0)throw Error("write-data",1,false,errno);n+=size_t(k);}if(flush&&fsync(fd))throw Error("write-fsync",1,false,errno);}
static void discard_owned(const std::string&p,int fd){
    struct stat owner,current;
    if(fstat(fd,&owner)||lstat(p.c_str(),&current)||owner.st_dev!=current.st_dev||owner.st_ino!=current.st_ino||unlink(p.c_str()))throw Error("temporary-cleanup-unconfirmed",90,true);
}
static void cleanup_preserving_error(const std::string&p,int fd,const std::exception&primary){
    auto original=dynamic_cast<const Error*>(&primary);
    fprintf(stderr,"FILE_PRIMARY step=%s rc=%d errno=%d\n",primary.what(),original?original->rc:1,original?original->system_errno:0);
    try{discard_owned(p,fd);}catch(const std::exception&cleanup){
        fprintf(stderr,"FILE_CLEANUP_UNCONFIRMED step=%s restore_rc=90\n",cleanup.what());
        throw Error(primary.what(),original?original->rc:1,true,original?original->system_errno:0);
    }
}
void write_new(const std::string&p,const std::string&s){FD fd(open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));require(fd.n>=0,"create:"+p);try{write_fd(fd.n,s);}catch(const std::exception&e){cleanup_preserving_error(p,fd.n,e);throw;}}
void write_replace(const std::string&p,const std::string&s){
    const auto next=p+".native-next";
    FD owner(open(next.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));require(owner.n>=0,"replace-owned-create");
    try{write_fd(owner.n,s);if(exists(p))require(regular(p),"replace-type:"+p);require(!rename(next.c_str(),p.c_str()),"replace:"+p);}
    catch(const std::exception&e){cleanup_preserving_error(next,owner.n,e);throw;}
}
void remove_regular(const std::string&p){require(regular(p),"remove-type:"+p);require(!unlink(p.c_str()),"remove:"+p);}
void append(const std::string&p,const std::string&s){FD fd(open(p.c_str(),O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600));require(fd.n>=0,"append-open");struct stat st;require(!fstat(fd.n,&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"append-type");write_fd(fd.n,s);}
void mkdir_new(const std::string&p){require(!mkdir(p.c_str(),0700),"mkdir-exclusive");}
bool same_device(const std::string&a,const std::string&b){struct stat x,y;return !lstat(a.c_str(),&x)&&!lstat(b.c_str(),&y)&&x.st_dev==y.st_dev;}
void move_new(const std::string&from,const std::string&to,bool nonempty){
    cancelled();
    require(regular(from,nonempty)&&!exists(to),"move-preconditions");
    // 目标内核/文件系统若不支持 NOREPLACE 则失败，不回退到可能覆盖的 rename。
    if(syscall(SYS_renameat2,AT_FDCWD,from.c_str(),AT_FDCWD,to.c_str(),1))throw Error("rename-noreplace",1,false,errno);
}
// 仅成片发布允许旧 exFAT 的复制回退；安装等其它移动仍要求原子 NOREPLACE。
void move_publish_new(const std::string&from,const std::string&to){
    try{move_new(from,to);return;}catch(const Error&e){
        if(e.system_errno!=ENOSYS&&e.system_errno!=EINVAL&&e.system_errno!=EOPNOTSUPP)throw;
        fprintf(stderr,"PUBLISH_MOVE_COPY_FALLBACK errno=%d\n",e.system_errno);
    }
    cancelled();
    FD input(open(from.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));
    if(input.n<0)throw Error("publish-copy-input",1,false,errno);
    struct stat before{},after{};
    require(!fstat(input.n,&before)&&S_ISREG(before.st_mode)&&before.st_size>0,"publish-copy-type");
    FD output(open(to.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
    if(output.n<0)throw Error("publish-copy-exclusive",1,false,errno);
    try{
        char b[65536];off_t total=0;
        for(;;){cancelled();ssize_t n=::read(input.n,b,sizeof b);if(n<0&&errno==EINTR)continue;
            if(n<0)throw Error("publish-copy-read",1,false,errno);if(!n)break;
            write_fd(output.n,std::string(b,size_t(n)),false);total+=n;}
        if(fsync(output.n))throw Error("publish-copy-fsync",1,false,errno);
        require(total==before.st_size&&!fstat(input.n,&after)&&after.st_size==before.st_size&&after.st_mtime==before.st_mtime,"publish-copy-source-changed");
        require(!lstat(from.c_str(),&after)&&after.st_dev==before.st_dev&&after.st_ino==before.st_ino,"publish-copy-source-replaced");
        cancelled();
        if(unlink(from.c_str()))throw Error("publish-copy-remove-source",1,false,errno);
    }catch(const std::exception&e){cleanup_preserving_error(to,output.n,e);throw;}
}
void copy_new(const std::string&from,const std::string&to){
    FD input(open(from.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));require(input.n>=0,"copy-input");struct stat st;require(!fstat(input.n,&st)&&S_ISREG(st.st_mode),"copy-type");
    FD output(open(to.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));require(output.n>=0,"copy-output");
    try{char b[65536];for(;;){cancelled();ssize_t n=::read(input.n,b,sizeof b);if(n<0&&errno==EINTR)continue;if(n<0)throw Error("copy-read",1,false,errno);if(!n)break;write_fd(output.n,std::string(b,size_t(n)),false);}if(fsync(output.n))throw Error("copy-fsync",1,false,errno);}
    catch(const std::exception&e){cleanup_preserving_error(to,output.n,e);throw;}
}
std::string sha256(const std::string&p){
    FD fd(open(p.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));require(fd.n>=0,"hash-open");struct stat st;require(!fstat(fd.n,&st)&&S_ISREG(st.st_mode),"hash-type");
    ReceiptSha hash;rsha_init(&hash);unsigned char b[65536];for(;;){cancelled();ssize_t n=::read(fd.n,b,sizeof b);if(n<0&&errno==EINTR)continue;require(n>=0,"hash-read");if(!n)break;rsha_update(&hash,b,unsigned(n));}
    unsigned char digest[32];rsha_final(&hash,digest);std::string out;const char h[]="0123456789abcdef";for(auto n:digest){out+=h[n>>4];out+=h[n&15];}return out;
}
bool mapped_library(const std::vector<std::string>&names){
    DIR*d=opendir("/proc");require(d!=nullptr,"proc-open");bool found=false;
    try{while(auto*e=readdir(d)){if(!digits(e->d_name))continue;const std::string process=std::string("/proc/")+e->d_name;try{auto maps=read(process+"/maps",4*1024*1024);for(const auto&n:names)if(maps.find(n)!=std::string::npos)found=true;}catch(const Error&){if(!access(process.c_str(),F_OK)||errno!=ENOENT)throw;}if(found)break;}}
    catch(...){closedir(d);throw;}
    closedir(d);return found;
}
bool digits(const std::string&s){return !s.empty()&&std::all_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';});}
static bool allowed_program(const std::string&p){
    return p=="/bin/dbus-send"||p=="/system/bin/getprop"||p=="/system/bin/setprop"||p=="/system/bin/start"||p=="/system/bin/stop";
}
Process command(const std::vector<std::string>&args,unsigned timeout,bool honour_cancel){
    const bool fixed_mount=args.size()==4&&args[0]=="/system/bin/toybox"&&args[1]=="mount"&&
        (args[2]=="-oremount,ro"||args[2]=="-oremount,rw")&&args[3]=="/system";
    require(!args.empty()&&(allowed_program(args[0])||fixed_mount),"program-not-allowed",2);
    require(args.size()<=32&&timeout>0&&timeout<=180000,"command-bounds",2);
    for(const auto&s:args)require(s.size()<2048&&s.find('\0')==std::string::npos,"argument-bounds",2);
    if(honour_cancel)cancelled();
    int pipefd[2];require(!pipe2(pipefd,O_CLOEXEC),"pipe");FD output(pipefd[0]),writer(pipefd[1]);
    int flags=fcntl(output.n,F_GETFL);require(flags>=0&&!fcntl(output.n,F_SETFL,flags|O_NONBLOCK),"pipe-nonblocking");
    uint64_t start=monotonic_ms();
    pid_t child=fork();require(child>=0,"fork");
    if(!child){
        signal(SIGHUP,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);
        int dup=fcntl(writer.n,F_DUPFD_CLOEXEC,3);if(dup<0||dup2(dup,1)<0)_exit(126);close(dup);
        if(output.n!=1)close(output.n);if(writer.n!=1)close(writer.n);
        std::vector<char*> av;for(const auto&s:args)av.push_back(const_cast<char*>(s.c_str()));av.push_back(nullptr);
        execve(av[0],av.data(),environ);_exit(127);
    }
    close(writer.n);writer.n=-1;
    Process result={1,{}};bool done=false,eof=false,killed=false;int status=0;uint64_t term=0;
    auto terminate=[&](){if(!killed){if(!done)kill(child,SIGTERM);killed=true;term=monotonic_ms();}};
    try{
        while(!done||!eof){
            char b[4096];ssize_t n=::read(output.n,b,sizeof b);
            if(n>0){require(result.output.size()+size_t(n)<=1024*1024,"command-output-limit");result.output.append(b,size_t(n));}
            else if(!n)eof=true;
            else require(errno==EAGAIN||errno==EINTR,"command-read");
            if(!done){pid_t got=waitpid(child,&status,WNOHANG);require(got>=0||errno==EINTR,"waitpid");done=got==child;}
            auto now=monotonic_ms();
            if((honour_cancel&&stopping())||now-start>=timeout)terminate();
            if(killed&&!done&&now-term>=1000)kill(child,SIGKILL);
            if(killed&&done)break;
            if(!done||!eof){pollfd p={output.n,POLLIN,0};poll(&p,1,10);}
        }
    }catch(...){
        if(!done){kill(child,SIGKILL);while(waitpid(child,&status,0)<0&&errno==EINTR){}}
        throw;
    }
    if(honour_cancel&&stopping())throw Error("cancelled",143);
    if(killed)throw Error("command-timeout",124);
    result.rc=WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?128+WTERMSIG(status):1;
    return result;
}
std::string get(const std::string&group,const std::string&property,const std::string&type,unsigned timeout){
    require(group=="camera"||group=="storage"||group=="error"||group=="system","dbus-group",2);
    require(!property.empty()&&property.size()<80&&property.find_first_not_of("abcdefghijklmnopqrstuvwxyz_")==std::string::npos,"dbus-property",2);
    const std::string dest="com.hasselblad."+group;
    auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout="+std::to_string(timeout),"--dest="+dest,"/"+group,"org.freedesktop.DBus.Properties.Get","string:"+dest,"string:"+property},timeout+1000);
    require(!r.rc,"dbus-get:"+group+"."+property,r.rc);
    std::istringstream lines(r.output);std::string line,result;unsigned count=0;
    while(std::getline(lines,line)){std::istringstream fields(line);std::string tag,kind,v,extra;fields>>tag;if(tag!="variant")continue;count++;fields>>kind>>v;require(kind==type&&!v.empty()&&!(fields>>extra),"dbus-variant:"+property);result=v;}
    require(count==1,"dbus-variant-count:"+property);return result;
}
std::string getprop(const std::string&p){require(p=="persist.hbl.dcf.fileindex"||p=="persist.hbl.dcf.folderindex","property-not-allowed",2);auto r=command({"/system/bin/getprop",p});require(!r.rc,"getprop",r.rc);auto s=trim_newlines(r.output);require(digits(s)&&s.size()<=10,"getprop-value",2);return s;}
void setprop(const std::string&p,const std::string&v){require(p=="persist.hbl.dcf.fileindex"||p=="persist.hbl.dcf.folderindex","property-not-allowed",2);require(digits(v)&&v.size()<=10,"property-value",2);auto r=command({"/system/bin/setprop",p,v});require(!r.rc,"setprop",r.rc);}
void service(const std::string&name,bool start){require(name=="camera-storage"||name=="camera-service"||name=="camera-gui","service-not-allowed",2);auto r=command({start?"/system/bin/start":"/system/bin/stop",name});require(!r.rc,start?"start-service":"stop-service",r.rc);}
std::string executable(pid_t pid){char p[64],b[4096];snprintf(p,sizeof p,"/proc/%ld/exe",long(pid));ssize_t n=readlink(p,b,sizeof b-1);if(n<0)return {};return std::string(b,size_t(n));}
std::vector<pid_t> pids(const std::string&name){
    std::vector<pid_t> out;DIR*d=opendir("/proc");require(d!=nullptr,"proc-open");
    try{for(;;){
        errno=0;auto*e=readdir(d);if(!e){if(errno)throw Error("proc-enumeration",1,false,errno);break;}
        if(!digits(e->d_name))continue;long p=strtol(e->d_name,nullptr,10);if(p<=0||p>INT_MAX)continue;
        const auto root=std::string("/proc/")+e->d_name;
        try{
            auto stat=read(root+"/stat",16384);auto end=stat.rfind(')');require(end!=std::string::npos,"proc-stat");std::istringstream fields(stat.substr(end+1));std::string item,state,flags;
            for(unsigned field=3;field<=9;field++){require(bool(fields>>item),"proc-stat-fields");if(field==3)state=item;if(field==9)flags=item;}
            require(digits(flags),"proc-stat-flags");if(state=="Z"||(std::stoull(flags)&0x00200000u))continue;
            char buffer[4096];ssize_t n=readlink((root+"/exe").c_str(),buffer,sizeof(buffer));
            if(n<0){
                const int cause=errno;
                // Unrelated Android daemons can deny exe readlink even to this
                // process. A matching (possibly truncated) comm stays fail-closed.
                const auto begin=stat.find('(');
                require(begin!=std::string::npos&&begin<end,"proc-stat-name");
                const auto comm=stat.substr(begin+1,end-begin-1);
                if((cause==EACCES||cause==EPERM||cause==ENOENT)&&comm!=name.substr(0,15))continue;
                throw Error("proc-executable-unconfirmed",1,false,cause);
            }
            require(size_t(n)<sizeof(buffer),"proc-executable-limit");
            std::string path(buffer,size_t(n));auto slash=path.find_last_of('/');if(slash!=std::string::npos&&path.substr(slash+1)==name)out.push_back(pid_t(p));
        }catch(const Error&){if(!access(root.c_str(),F_OK)||errno!=ENOENT)throw;}
    }}catch(...){closedir(d);throw;}
    closedir(d);return out;
}
Album album_plan(const std::string&s){
    require(s.size()==21,"album-plan-length",2);
    require(s[0]>='1'&&s[0]<='9'&&digits(s.substr(0,3))&&s.substr(3,6)=="HASBL/"&&s[9]>='A'&&s[9]<='Z'&&digits(s.substr(10,7))&&s.substr(17)==".JPG","album-plan-format",2);
    return {s.substr(0,8),s.substr(9,8),unsigned(std::stoul(s.substr(0,3))),unsigned(std::stoul(s.substr(10,7)))};
}
}
