/* 第一代 X2D 4.2.0 / H3 过渡事务。固定表仍含历史业务脚本，非最终发行包。 */
#include "native_common.hpp"
#include "native_install_tables.hpp"
#ifndef CN_INSTALL_FEATURE_MASK
#define CN_INSTALL_FEATURE_MASK 255
#endif
#include "receipt_sha256.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <memory>
#include <sstream>
#include <set>
#include <algorithm>
#include <exception>
#include <limits>

extern "C" int cn_signal_pending(void);
namespace cn { namespace install {
static constexpr const char* stage_path="/data/local/tmp/x2d-rs1";
static constexpr const char* check_path="/system/bin/x2d-rs-check-r1";
static constexpr const char* task_name="x2d-multishot-active";
static void ok(bool success,const std::string&step,int rc=1){if(!success)throw Error(step,rc,false,errno);}
struct FD {
    int n=-1;explicit FD(int value=-1):n(value){}~FD(){if(n>=0)close(n);}
    FD(const FD&)=delete;FD&operator=(const FD&)=delete;
    FD(FD&&o)noexcept:n(o.n){o.n=-1;}FD&operator=(FD&&o)noexcept{if(n>=0)close(n);n=o.n;o.n=-1;return *this;}
};
static bool hex16(const std::string&s){return s.size()==16&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static void name(const std::string&s){require(!s.empty()&&s!="."&&s!=".."&&s.find('/')==std::string::npos&&s.find('\0')==std::string::npos,"install-component",2);}
static FD open_dir(const std::string&path,bool create=false){
    require(path.size()>1&&path[0]=='/'&&path.back()!='/',"install-directory-path",2);
    FD fd(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ok(fd.n>=0,"install-root");
    std::istringstream input(path.substr(1));std::string part;
    while(std::getline(input,part,'/')){name(part);cancelled();if(create&&mkdirat(fd.n,part.c_str(),0755))ok(errno==EEXIST,"install-parent-create");FD next(openat(fd.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ok(next.n>=0,"install-parent-type:"+part);fd=std::move(next);}return fd;
}
static FD child_dir(int parent,const std::string&s){name(s);FD fd(openat(parent,s.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ok(fd.n>=0,"install-subdirectory:"+s);return fd;}
static bool present(int parent,const std::string&s){name(s);struct stat st;if(!fstatat(parent,s.c_str(),&st,AT_SYMLINK_NOFOLLOW))return true;ok(errno==ENOENT,"install-stat:"+s);return false;}
static struct stat identity(int fd){struct stat st;ok(!fstat(fd,&st),"install-fstat");return st;}
static bool same(const struct stat&a,const struct stat&b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino;}
static void verify_identity(int parent,const std::string&s,const struct stat&owner){struct stat st;ok(!fstatat(parent,s.c_str(),&st,AT_SYMLINK_NOFOLLOW),"install-object-state:"+s);require(same(st,owner),"install-object-replaced:"+s,90);}
static void remove_owned(int parent,const std::string&s,const struct stat&owner,bool directory=false){verify_identity(parent,s,owner);ok(!unlinkat(parent,s.c_str(),directory?AT_REMOVEDIR:0),"install-remove:"+s,90);ok(!fsync(parent),"install-remove-sync",90);}
static FD file(int parent,const std::string&s){name(s);FD fd(openat(parent,s.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));ok(fd.n>=0,"install-file-open:"+s);auto st=identity(fd.n);require(S_ISREG(st.st_mode)&&st.st_nlink==1,"install-file-type:"+s);return fd;}
static std::string hash_fd(int fd){
    auto before=identity(fd);ok(lseek(fd,0,SEEK_SET)>=0,"install-hash-seek");ReceiptSha h;rsha_init(&h);unsigned char b[65536];
    for(;;){cancelled();ssize_t n=::read(fd,b,sizeof b);if(n<0&&errno==EINTR)continue;ok(n>=0,"install-hash-read");if(!n)break;rsha_update(&h,b,unsigned(n));}
    auto after=identity(fd);require(before.st_size==after.st_size&&before.st_mtim.tv_sec==after.st_mtim.tv_sec&&before.st_mtim.tv_nsec==after.st_mtim.tv_nsec&&after.st_nlink==1,"install-hash-changed");
    unsigned char raw[32];rsha_final(&h,raw);std::string out;for(auto v:raw){out+="0123456789abcdef"[v>>4];out+="0123456789abcdef"[v&15];}return out;
}
static std::string hash(int dir,const std::string&s){auto fd=file(dir,s);return hash_fd(fd.n);}
static void write_fd(int fd,const char*data,size_t size){size_t at=0;while(at<size){ssize_t n=write(fd,data+at,size-at);if(n<0&&errno==EINTR)continue;ok(n>0,"install-write");at+=size_t(n);}ok(!fsync(fd),"install-file-sync");}
struct Owned {std::string name;struct stat id{};bool created=false;};
static void create_text(int dir,Owned&out,const std::string&s,const std::string&text){
    name(s);require(!out.created,"install-owner-reuse");out.name=s;
    FD f(openat(dir,s.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));ok(f.n>=0,"install-create:"+s);
    out.created=true;out.id=identity(f.n);write_fd(f.n,text.data(),text.size());ok(!fsync(dir),"install-create-sync");
}
static void copy_file(int source,const std::string&src,int dest,Owned&out,const std::string&dst,unsigned mode,const std::string&expected){
    auto input=file(source,src);require(hash_fd(input.n)==expected,"install-source-digest:"+src);ok(lseek(input.n,0,SEEK_SET)>=0,"install-source-seek");name(dst);out.name=dst;
    FD target(openat(dest,dst.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));ok(target.n>=0,"install-target-create:"+dst);out.created=true;out.id=identity(target.n);
    char b[65536];for(;;){cancelled();ssize_t n=::read(input.n,b,sizeof b);if(n<0&&errno==EINTR)continue;ok(n>=0,"install-copy-read");if(!n)break;size_t at=0;while(at<size_t(n)){ssize_t m=write(target.n,b+at,size_t(n)-at);if(m<0&&errno==EINTR)continue;ok(m>0,"install-copy-write");at+=size_t(m);}}
    ok(!fchmod(target.n,mode),"install-target-mode");ok(!fsync(target.n),"install-target-sync");require(hash_fd(target.n)==expected&&hash_fd(input.n)==expected,"install-copy-digest");ok(!fsync(dest),"install-parent-sync");
}
static void atomic_new_text(int dir,const std::string&dest,const std::string&text){
    Owned next;try{create_text(dir,next,dest+".native-next",text);ok(!syscall(SYS_renameat2,dir,next.name.c_str(),dir,dest.c_str(),1),"install-receipt-noreplace");next.created=false;ok(!fsync(dir),"install-receipt-sync");}
    catch(const std::exception&e){auto cause=dynamic_cast<const Error*>(&e);fprintf(stderr,"INSTALL_RECEIPT_PRIMARY step=%s rc=%d errno=%d\n",e.what(),cause?cause->rc:1,cause?cause->system_errno:0);if(next.created){try{remove_owned(dir,next.name,next.id);}catch(const std::exception&cleanup){fprintf(stderr,"INSTALL_RECEIPT_CLEANUP_UNCONFIRMED step=%s restore_rc=90\n",cleanup.what());throw Error(e.what(),cause?cause->rc:1,true,cause?cause->system_errno:0);}}throw;}
}
static void remount(bool ro){
    std::istringstream lines(read("/proc/self/mounts",1024*1024));std::string line;unsigned count=0;unsigned long flags=MS_REMOUNT|(ro?MS_RDONLY:0);
    while(std::getline(lines,line)){std::istringstream row(line);std::string src,target,type,options;require(bool(row>>src>>target>>type>>options),"install-mount-record");if(target!="/system")continue;++count;std::istringstream opts(options);std::string v;while(std::getline(opts,v,',')){if(v=="nosuid")flags|=MS_NOSUID;else if(v=="nodev")flags|=MS_NODEV;else if(v=="noexec")flags|=MS_NOEXEC;else if(v=="sync")flags|=MS_SYNCHRONOUS;else if(v=="dirsync")flags|=MS_DIRSYNC;else if(v=="noatime")flags|=MS_NOATIME;else if(v=="nodiratime")flags|=MS_NODIRATIME;else if(v=="relatime")flags|=MS_RELATIME;else if(v=="strictatime")flags|=MS_STRICTATIME;}}
    require(count==1,"install-system-mount-ambiguous");ok(!system_remount(flags),"install-system-remount",90);
    std::istringstream verify(read("/proc/self/mounts",1024*1024));count=0;while(std::getline(verify,line)){std::istringstream row(line);std::string a,b,c,d;require(bool(row>>a>>b>>c>>d),"install-mount-verify-record",90);if(b=="/system"){++count;require((","+d+",").find(ro?",ro,":",rw,")!=std::string::npos,"install-system-remount-verify",90);}}require(count==1,"install-system-mount-lost",90);
}
static bool idle(){return get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("system","system_state","int32")=="1";}
static void live(bool enabled){auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.set_live_view",enabled?"boolean:true":"boolean:false"},6000);require(!r.rc,"install-live-view",r.rc);require(get("camera","live_view_state","int32")== (enabled?"1":"0"),"install-live-view-unconfirmed");}
template<class F>static void wait_for(F&&f,unsigned ms,const char*step){auto end=monotonic_ms()+ms;for(;;){cancelled();try{if(f())return;}catch(const Error&e){if(e.rc==143)throw;}require(monotonic_ms()<end,step);sleep_ms(100);}}
struct Proc {pid_t pid;std::string start,exe;};
static std::vector<Proc> processes(const std::string&comm){
    DIR*d=opendir("/proc");ok(d!=nullptr,"install-proc-open");std::vector<Proc> out;
    try{for(;;){errno=0;auto*e=readdir(d);if(!e){ok(!errno,"install-proc-enumeration");break;}cancelled();if(!digits(e->d_name))continue;std::string prefix=std::string("/proc/")+e->d_name;
        try{if(trim_newlines(read(prefix+"/comm",128))!=comm)continue;auto stat=read(prefix+"/stat",8192);auto close=stat.rfind(')');require(close!=std::string::npos,"install-process-stat");std::istringstream fields(stat.substr(close+2));std::string part,state,start;for(unsigned field=3;field<=22;++field){require(bool(fields>>part),"install-process-stat-fields");if(field==3)state=part;if(field==22)start=part;}if(state=="Z")continue;
            auto pid=std::stoul(e->d_name);require(pid>0&&pid<=2147483647,"install-process-pid");auto exe=executable(pid_t(pid));require(!exe.empty(),"install-process-exe");out.push_back({pid_t(pid),start,exe});
        }catch(const Error&){if(!access(prefix.c_str(),F_OK)||errno!=ENOENT)throw;}
    }}catch(...){closedir(d);throw;}closedir(d);return out;
}
static void stop_services(){std::exception_ptr first;for(const auto*n:{"camera-gui","camera-service"})try{service(n,false);}catch(...){if(!first)first=std::current_exception();}try{wait_for([]{return processes("camera-gui").empty()&&processes("camera-service").empty();},5000,"install-services-stop-timeout");}catch(...){if(!first)first=std::current_exception();}if(first)std::rethrow_exception(first);}
static void start_services(){service("camera-service",true);service("camera-gui",true);wait_for([]{return !processes("camera-gui").empty()&&!processes("camera-service").empty()&&idle();},30000,"install-services-start-timeout");}
static const Target&target(const std::string&source){for(const auto&t:targets)if(source==t.source)return t;throw Error("install-fixed-target-missing");}
static void stop_own_workers(){
    auto list=processes("worker");if(list.empty())return;const auto&t=target("p07-worker");
    for(const auto&p:list){require(p.exe==t.path,"install-unrelated-worker",90);
        // 唯一允许跟随的链接是内核 /proc/PID/exe，复核实际映射的可执行文件。
        FD exe(open(("/proc/"+std::to_string(p.pid)+"/exe").c_str(),O_RDONLY|O_CLOEXEC));
        ok(exe.n>=0,"install-worker-executable",90);auto st=identity(exe.n);require(S_ISREG(st.st_mode)&&st.st_nlink==1&&hash_fd(exe.n)==t.digest,"install-worker-digest",90);
        bool current=false;for(const auto&again:processes("worker"))if(again.pid==p.pid){require(again.start==p.start&&again.exe==p.exe,"install-worker-reused",90);current=true;}
        if(current)ok(!kill(p.pid,SIGTERM)||errno==ESRCH,"install-worker-stop",90);
    }
    wait_for([]{return processes("worker").empty();},5000,"install-worker-stop-timeout");
}
struct ProbeResult {int rc;std::string text;};
[[maybe_unused]] static ProbeResult probe(const std::string&exe,bool preload){
    require(exe==std::string(check_path)+"/probe"||exe==std::string(check_path)+"/core-check"||exe==std::string(check_path)+"/renderer","install-probe-command",2);
    int pipefd[2];ok(!pipe2(pipefd,O_CLOEXEC|O_NONBLOCK),"install-probe-pipe");FD input(pipefd[0]),output(pipefd[1]);pid_t pid=fork();ok(pid>=0,"install-probe-fork");
    if(!pid){close(input.n);int copy=fcntl(output.n,F_DUPFD_CLOEXEC,3);if(copy<0)_exit(126);int flags=fcntl(copy,F_GETFL);if(flags<0||fcntl(copy,F_SETFL,flags&~O_NONBLOCK)||dup2(copy,1)<0||dup2(copy,2)<0)_exit(126);close(copy);if(output.n!=1&&output.n!=2)close(output.n);struct rlimit core={0,0};if(setrlimit(RLIMIT_CORE,&core))_exit(126);
        char *args[]={const_cast<char*>(exe.c_str()),nullptr};char *env[]={const_cast<char*>("PATH=/system/bin:/system/xbin"),const_cast<char*>("X2D_RAW_STACK_ABI="),preload?const_cast<char*>("LD_PRELOAD=/system/bin/x2d-rs-check-r1/library.so"):nullptr,nullptr};execve(exe.c_str(),args,env);_exit(127);}
    output=FD();std::string text;bool done=false,eof=false,term=false,killed=false;int status=0,primary=0;uint64_t started=monotonic_ms(),sent=0;
    try{while(!done||!eof){
        auto now=monotonic_ms();if(!done&&!term&&(cn_signal_pending()||now-started>=30000)){primary=cn_signal_pending()?143:124;ok(!kill(pid,SIGTERM)||errno==ESRCH,"install-probe-term");term=true;sent=now;}
        if(term&&!done&&!killed&&now-sent>=250){ok(!kill(pid,SIGKILL)||errno==ESRCH,"install-probe-kill");killed=true;}
        if(term&&now-sent>=2250&&!done)throw Error("install-probe-reap-unconfirmed",primary?primary:124,true);
        char b[4096];for(unsigned reads=0;reads<16;++reads){ssize_t n=::read(input.n,b,sizeof b);if(n>0){if(text.size()+size_t(n)>65536){primary=1;if(!done&&!term){ok(!kill(pid,SIGTERM)||errno==ESRCH,"install-probe-output-term");term=true;sent=now;}}else text.append(b,size_t(n));continue;}if(!n)eof=true;else if(errno!=EINTR&&errno!=EAGAIN)throw Error("install-probe-read",1,false,errno);break;}
        if(!done){pid_t r=waitpid(pid,&status,WNOHANG);if(r==pid)done=true;else if(r<0&&errno!=EINTR)throw Error("install-probe-wait",1,true,errno);}
        if(done&&!eof&&now-started>=32000)throw Error("install-probe-pipe-unclosed",1,true);
        if(!done||!eof){pollfd p={input.n,POLLIN,0};if(poll(&p,1,20)<0&&errno!=EINTR)throw Error("install-probe-poll",1,false,errno);}
    }}catch(const std::exception&e){fprintf(stderr,"INSTALL_PROBE_PRIMARY step=%s\n",e.what());if(!done){kill(pid,SIGKILL);auto end=monotonic_ms()+2000;for(;;){pid_t r=waitpid(pid,&status,WNOHANG);if(r==pid){done=true;break;}if(r<0&&errno!=EINTR)break;if(monotonic_ms()>=end)break;timespec delay={0,10000000};nanosleep(&delay,nullptr);}}if(!done)throw Error("install-probe-reap-unconfirmed",1,true);throw;}
    if(primary)throw Error("install-probe-interrupted-or-timeout",primary);return {WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status),text};
}
struct Item {const Target*spec;std::string base,parent;FD dir;Owned next,rollback;struct stat switched_id{};bool switched=false;};
struct State {
    FD stage,payload,backup,runtime,bin,check;std::vector<Item> items;std::vector<Owned> check_files;
    Owned task,started;struct stat check_id{};bool check_created=false,check_identified=false,paused=false,stopped=false,changed=false,files_complete=false,probe_unsafe=false;
    int primary=0,restore=0,primary_errno=0;std::string primary_step,restore_steps,phase="preflight",attempt;
};
static void recover_error(State&s,const std::string&step,const std::exception&e){s.restore=90;if(!s.restore_steps.empty())s.restore_steps+=",";s.restore_steps+=step;fprintf(stderr,"INSTALL_RECOVERY_FAILED step=%s detail=%s\n",step.c_str(),e.what());}
static void guards_ok(){for(const auto&g:guards){std::string p=g.path;auto slash=p.find_last_of('/');auto dir=open_dir(p.substr(0,slash));require(hash(dir.n,p.substr(slash+1))==g.digest,"install-original-guard:"+p);}}
static void current_original(const Item&i){if(std::string(i.spec->original)=="ABSENT")require(!present(i.dir.n,i.base),"install-target-conflict:"+i.base);else require(hash(i.dir.n,i.base)==i.spec->original,"install-original-digest:"+i.base);}
static void ensure_parent(Item&i,bool create){i.dir=open_dir(i.parent,create);}
static void cleanup_checks(State&s){
    if(!s.check_created)return;require(s.check_identified&&!s.probe_unsafe,"install-check-cleanup-unconfirmed",90);
    if(s.check.n<0)s.check=child_dir(s.bin.n,"x2d-rs-check-r1");
    FD directory=std::move(s.check);require(same(identity(directory.n),s.check_id),"install-check-directory-replaced",90);
    for(auto&f:s.check_files)if(f.created){remove_owned(directory.n,f.name,f.id);f.created=false;}
    // 已删除但仍打开的目录会使 /system remount,ro 返回 EBUSY。
    directory=FD();
    remove_owned(s.bin.n,"x2d-rs-check-r1",s.check_id,true);s.check_created=false;
}
static void system_space(){
    auto system=open_dir("/system");struct statvfs space;ok(!fstatvfs(system.n,&space),"install-system-space-read",2);
    uint64_t unit=space.f_frsize,blocks=space.f_bavail;require(unit&&blocks<=std::numeric_limits<uint64_t>::max()/unit,"install-system-space-overflow",2);
    require(payload_bytes<=(std::numeric_limits<uint64_t>::max()-1048576u)/2,"install-required-space-overflow",2);
    uint64_t needed=2*payload_bytes+1048576u;require(runtime_check_bytes<=std::numeric_limits<uint64_t>::max()-1048576u,"install-check-space-overflow",2);needed=std::max<uint64_t>(needed,runtime_check_bytes+uint64_t(1048576u));
    fprintf(stdout,"INSTALL_SYSTEM_FREE_BYTES=%llu INSTALL_SYSTEM_REQUIRED_BYTES=%llu\n",(unsigned long long)(blocks*unit),(unsigned long long)needed);
    require(blocks*unit>=needed,"install-system-space-insufficient",2);
}
static void no_installed_mappings(const State&s){
    DIR*proc=opendir("/proc");ok(proc!=nullptr,"install-map-proc-open",90);
    try{for(;;){errno=0;auto*e=readdir(proc);if(!e){ok(!errno,"install-map-enumeration",90);break;}if(!digits(e->d_name))continue;std::string p=std::string("/proc/")+e->d_name;
        try{std::istringstream lines(read(p+"/maps",16*1024*1024));std::string line;while(std::getline(lines,line)){std::istringstream row(line);std::string range,perms,off,device,inode;require(bool(row>>range>>perms>>off>>device>>inode)&&digits(inode),"install-map-record",90);auto colon=device.find(':');require(colon!=std::string::npos,"install-map-device",90);size_t used=0;auto maj=std::stoul(device.substr(0,colon),&used,16);require(used==colon,"install-map-major",90);auto mintext=device.substr(colon+1);auto min=std::stoul(mintext,&used,16);require(used==mintext.size(),"install-map-minor",90);auto ino=std::stoull(inode);
            for(const auto&i:s.items)if(i.switched&&i.switched_id.st_ino==ino&&major(i.switched_id.st_dev)==maj&&minor(i.switched_id.st_dev)==min)throw Error("install-rollback-library-still-mapped",90);
        }}catch(const Error&e2){if(e2.rc==90)throw;if(!access(p.c_str(),F_OK)||errno!=ENOENT)throw Error("install-map-evidence-unconfirmed",90);}
    }}catch(...){closedir(proc);throw;}closedir(proc);
}
static void rollback(State&s){
    for(auto it=s.items.rbegin();it!=s.items.rend();++it){auto&i=*it;if(!i.switched)continue;
        verify_identity(i.dir.n,i.base,i.switched_id);require(hash(i.dir.n,i.base)==i.spec->digest,"install-rollback-target-changed",90);
        if(std::string(i.spec->original)=="ABSENT")remove_owned(i.dir.n,i.base,i.switched_id);
        else{copy_file(s.backup.n,i.spec->source,i.dir.n,i.rollback,i.base+".rs-rollback",i.spec->mode,i.spec->original);verify_identity(i.dir.n,i.base,i.switched_id);ok(!renameat(i.dir.n,i.rollback.name.c_str(),i.dir.n,i.base.c_str()),"install-rollback-rename",90);i.rollback.created=false;ok(!fsync(i.dir.n),"install-rollback-sync",90);require(hash(i.dir.n,i.base)==i.spec->original,"install-rollback-verify",90);}i.switched=false;
    }
}
static void preserve_failed_menu_attempt(State&s){
    const std::string marker="x2d-native-menu-attempt";
    if(!present(s.runtime.n,marker))return;
    auto old=file(s.runtime.n,marker);auto st=identity(old.n);require(st.st_size==0,"install-menu-attempt-not-empty",90);
    const std::string empty="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    Owned saved;copy_file(s.runtime.n,marker,s.stage.n,saved,"attempt-rollback",0600,empty);
    remove_owned(s.runtime.n,marker,st);
}
static void finish(State&s){
    Recovery recovery;
    if(s.primary&&s.stopped){try{stop_services();}catch(const std::exception&e){recover_error(s,"stop-services",e);}try{stop_own_workers();}catch(const std::exception&e){recover_error(s,"stop-worker",e);}}
    bool switched=std::any_of(s.items.begin(),s.items.end(),[](const Item&i){return i.switched;});
    if(s.primary&&switched){try{no_installed_mappings(s);s.changed=true;remount(false);rollback(s);guards_ok();}catch(const std::exception&e){recover_error(s,"rollback",e);}}
    if(s.changed){try{remount(false);for(auto&i:s.items){if(i.next.created){remove_owned(i.dir.n,i.next.name,i.next.id);i.next.created=false;}if(i.rollback.created){remove_owned(i.dir.n,i.rollback.name,i.rollback.id);i.rollback.created=false;}}cleanup_checks(s);}catch(const std::exception&e){recover_error(s,"temporary-cleanup",e);}try{remount(true);}catch(const std::exception&e){recover_error(s,"readonly",e);}}
    if(s.primary&&s.stopped&&!s.restore){try{preserve_failed_menu_attempt(s);}catch(const std::exception&e){recover_error(s,"preserve-menu-attempt",e);}try{start_services();}catch(const std::exception&e){recover_error(s,"start-services",e);}}
    if(s.paused&&!s.restore){try{live(true);s.paused=false;}catch(const std::exception&e){recover_error(s,"restore-live",e);}}
    // 回滚或原厂服务状态未确认时，不恢复取景/解锁以允许新任务。
    if(s.restore&&s.stopped){try{stop_services();}catch(const std::exception&e){recover_error(s,"stop-after-recovery-failure",e);}}
    if(s.task.created&&!s.restore){try{remove_owned(s.runtime.n,s.task.name,s.task.id,true);s.task.created=false;}catch(const std::exception&e){recover_error(s,"task-unlock",e);}}
    if(s.task.created)fprintf(stderr,"INSTALL_TASK_LOCK_RETAINED restore_rc=%d\n",s.restore);
    if(!s.primary&&cn_signal_pending()){s.primary=143;s.primary_step="cancelled-after-file-transaction";}
    int result=s.restore?s.restore:s.primary;
    try{
        if(!result)atomic_new_text(s.stage.n,"done","RAW_STACK_STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED\n");
        atomic_new_text(s.stage.n,"result",std::to_string(result)+"\n");
        std::string text="NATIVE_TRANSACTION_V1\noperation=install-stock\nattempt="+s.attempt+"\nfirmware=4.2.0\nfeature_mask="+std::to_string(CN_INSTALL_FEATURE_MASK)+"\nmanifest="+transition_manifest_sha+"\nprimary_rc="+std::to_string(s.primary)+"\nprimary_step="+s.primary_step+"\nprimary_errno="+std::to_string(s.primary_errno)+"\nrestore_rc="+std::to_string(s.restore)+"\nrestore_steps="+s.restore_steps+"\nfiles_complete="+(s.files_complete?"1":"0")+"\nfinal_rc="+std::to_string(result)+"\nmanual_reboot_required="+(!result?"1":"0")+"\n";
        atomic_new_text(s.stage.n,"install.final."+s.attempt,text);
    }catch(const std::exception&e){recover_error(s,"final-receipt",e);}
    fprintf(stderr,"INSTALL_FINISH primary_rc=%d restore_rc=%d files_complete=%d attempt=%s\n",s.primary,s.restore,s.files_complete,s.attempt.c_str());
}
static int run(const std::string&attempt){
    require(hex16(attempt),"install-attempt",2);State s;s.attempt=attempt;s.stage=open_dir(stage_path);s.runtime=open_dir("/tmp");s.bin=open_dir("/system/bin");
#ifdef CN_PACKAGED_PAYLOAD
    s.payload=child_dir(s.stage.n,"payload");
#else
    s.payload=open_dir(stage_path);
#endif
    // 在任何相机状态变化前拒绝重放与未知回执；host 必须显式保留旧 stage 后重建。
    for(const auto&f:{"backup","started","done","result"})require(!present(s.stage.n,f),"install-stage-not-fresh");
    require(!present(s.stage.n,"install.final."+attempt)&&!present(s.stage.n,"install.final."+attempt+".native-next"),"install-attempt-replayed");
    require(!present(s.bin.n,"x2d-rs-check-r1"),"install-check-directory-conflict");guards_ok();system_space();
    require(hash(s.payload.n,"check-raw-stack-arm")==core_check_sha&&hash(s.payload.n,"install-probe")==install_probe_sha,"install-check-source-digest");
    for(const auto&t:targets){require(hash(s.payload.n,t.source)==t.digest,"install-payload-digest:"+std::string(t.source));Item i;i.spec=&t;std::string path=t.path;auto split=path.find_last_of('/');i.parent=path.substr(0,split);i.base=path.substr(split+1);
        // 缺失的项目父目录留到 RW 阶段创建；所有已存在祖先仍必须逐级拒绝链接。
        try{ensure_parent(i,false);current_original(i);require(!present(i.dir.n,i.base+".rs-next")&&!present(i.dir.n,i.base+".rs-rollback"),"install-temporary-conflict");}
        catch(const Error&e){if(e.system_errno!=ENOENT||std::string(t.original)!="ABSENT")throw;}
        s.items.push_back(std::move(i));}
    require(idle(),"install-not-idle");require(processes("worker").empty(),"install-worker-present");
    create_text(s.stage.n,s.started,"started","NATIVE_INSTALL_STARTED\nattempt="+attempt+"\n");
    try{
        s.phase="task-lock";s.task.name=task_name;ok(!mkdirat(s.runtime.n,task_name,0700),"install-task-lock-busy");s.task.created=true;auto lock=child_dir(s.runtime.n,task_name);s.task.id=identity(lock.n);
        require(idle()&&processes("worker").empty(),"install-after-lock-not-idle");auto before=get("camera","live_view_state","int32");require(before=="0"||before=="1","install-live-state");
        s.phase="pause-view";if(before=="1"){s.paused=true;live(false);}wait_for([]{return idle()&&get("camera","stream_group","int32")=="255";},5000,"install-stream-idle-timeout");
        s.phase="backup";ok(!mkdirat(s.stage.n,"backup",0700),"install-backup-create");s.backup=child_dir(s.stage.n,"backup");
        for(auto&i:s.items)if(std::string(i.spec->original)!="ABSENT"){current_original(i);Owned saved;copy_file(i.dir.n,i.base,s.backup.n,saved,i.spec->source,0600,i.spec->original);}
#if CN_INSTALL_FEATURE_MASK == 255
        s.phase="runtime-checks";puts("INSTALL_STAGE=RUNTIME_CHECKS");s.changed=true;remount(false);ok(!mkdirat(s.bin.n,"x2d-rs-check-r1",0700),"install-check-create");s.check_created=true;s.check=child_dir(s.bin.n,"x2d-rs-check-r1");s.check_id=identity(s.check.n);s.check_identified=true;
        const char* src[]={"p00-libx2d_raw_stack.so","check-raw-stack-arm","p04-render-probe","install-probe"};const char* dst[]={"library.so","core-check","renderer","probe"};const char* sums[]={target(src[0]).digest,core_check_sha,target(src[2]).digest,install_probe_sha};
        s.check_files.resize(4);for(unsigned n=0;n<4;++n)copy_file(s.payload.n,src[n],s.check.n,s.check_files[n],dst[n],n?0755:0644,sums[n]);remount(true);
        try{auto loaded=probe(std::string(check_path)+"/probe",true);require(!loaded.rc&&loaded.text=="NATIVE_INSTALL_PRELOAD_MAPPED_HOOKS_OFF\n","install-runtime-preload");auto core=probe(std::string(check_path)+"/core-check",false);require(!core.rc,"install-runtime-core",core.rc);auto render=probe(std::string(check_path)+"/renderer",false);require(render.rc==1&&render.text.find("OBSERVER_EXIT 1\n")!=std::string::npos,"install-runtime-render");}
        catch(const Error&e){if(e.recovery_failed)s.probe_unsafe=true;throw;}
        remount(false);cleanup_checks(s);remount(true);require(idle(),"install-before-stop-not-idle");guards_ok();
#endif
        s.phase="services-stopping";puts("INSTALL_STAGE=SERVICES_STOPPING");system_space();s.stopped=true;stop_services();remount(false);
        s.phase="files-staging";for(auto&i:s.items){ensure_parent(i,true);current_original(i);require(!present(i.dir.n,i.base+".rs-next")&&!present(i.dir.n,i.base+".rs-rollback"),"install-late-temporary-conflict");copy_file(s.payload.n,i.spec->source,i.dir.n,i.next,i.base+".rs-next",i.spec->mode,i.spec->digest);}
        s.phase="files-switching";puts("INSTALL_STAGE=FILES_SWITCHING");for(auto&i:s.items){cancelled();current_original(i);verify_identity(i.dir.n,i.next.name,i.next.id);if(std::string(i.spec->original)=="ABSENT")ok(!syscall(SYS_renameat2,i.dir.n,i.next.name.c_str(),i.dir.n,i.base.c_str(),1),"install-switch-noreplace");else ok(!renameat(i.dir.n,i.next.name.c_str(),i.dir.n,i.base.c_str()),"install-switch-original");i.switched_id=i.next.id;i.switched=true;i.next.created=false;ok(!fsync(i.dir.n),"install-switch-sync");}
        remount(true);s.phase="services-starting";puts("INSTALL_STAGE=WAITING_SERVICES");start_services();puts("INSTALL_STAGE=SERVICES_READY");
        for(const auto&i:s.items){verify_identity(i.dir.n,i.base,i.switched_id);require(hash(i.dir.n,i.base)==i.spec->digest,"install-final-digest");}guards_ok();
        if(s.paused){live(true);s.paused=false;}require(idle(),"install-final-not-idle");s.files_complete=true;
    }catch(const Error&e){s.primary=e.rc;s.primary_step=s.phase+":"+e.what();s.primary_errno=e.system_errno;if(e.recovery_failed)s.restore=90;fprintf(stderr,"INSTALL_PRIMARY step=%s rc=%d errno=%d\n",s.primary_step.c_str(),s.primary,s.primary_errno);}
    catch(const std::exception&e){s.primary=1;s.primary_step=s.phase;fprintf(stderr,"INSTALL_PRIMARY phase=%s detail=%s\n",s.phase.c_str(),e.what());}
    finish(s);return s.restore?s.restore:s.primary;
}
} }
extern "C" int cn_install_main(int argc,char**argv){
    try{cn::require(argc==4&&std::string(argv[1])=="install-stock"&&std::string(argv[2])==std::to_string(CN_INSTALL_FEATURE_MASK),"install-arguments",2);cn::cancelled();cn::TransactionGuard transaction;return cn::install::run(argv[3]);}
    catch(const cn::Error&e){fprintf(stderr,"INSTALL_REFUSED step=%s rc=%d errno=%d\n",e.what(),e.rc,e.system_errno);return e.recovery_failed?90:e.rc;}
    catch(const std::exception&e){fprintf(stderr,"INSTALL_REFUSED detail=%s rc=1\n",e.what());return 1;}
}
