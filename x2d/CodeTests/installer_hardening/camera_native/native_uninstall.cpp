/* 固定 X2D 4.2.0 卸载事务。保留已映射对象的原 inode，等待手动重启。 */
#include "native_common.hpp"
#include "native_setup_tables.hpp"
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <memory>
extern "C" int cn_signal_pending(void);
extern char **environ;
namespace cn { namespace uninstall {
struct Parent {
    int fd=-1;std::string name;
    explicit Parent(const std::string&p,bool absent_parent=false){
        require(p.size()>1&&p[0]=='/'&&p.find('\0')==std::string::npos,"uninstall-path",2);
        fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);require(fd>=0,"uninstall-root");
        try{
            size_t begin=1;
            for(;;){auto end=p.find('/',begin);auto item=p.substr(begin,end==std::string::npos?end:end-begin);require(!item.empty()&&item!="."&&item!="..","uninstall-component",2);
                if(end==std::string::npos){name=item;break;}
                int next=openat(fd,item.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
                if(next<0&&errno==ENOENT&&absent_parent){close(fd);fd=-1;name=p.substr(p.find_last_of('/')+1);break;}
                require(next>=0,"uninstall-parent-type");close(fd);fd=next;begin=end+1;
            }
        }catch(...){if(fd>=0)close(fd);fd=-1;throw;}
    }
    ~Parent(){if(fd>=0)close(fd);}
    Parent(const Parent&)=delete;
    std::string bound()const{require(fd>=0,"uninstall-parent-missing");return "/proc/self/fd/"+std::to_string(fd)+"/"+name;}
    std::string state(struct stat*identity=nullptr)const{
        if(fd<0)return "ABSENT";struct stat st;
        if(fstatat(fd,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)){require(errno==ENOENT,"uninstall-state");return "ABSENT";}
        require(S_ISREG(st.st_mode)&&st.st_nlink==1,"uninstall-state-type");if(identity)*identity=st;return sha256(bound());
    }
};
static bool hex(const std::string&s,size_t n){return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static std::string identity(pid_t p){
    auto text=read("/proc/"+std::to_string(p)+"/stat",16384);auto end=text.rfind(')');require(end!=std::string::npos,"uninstall-process-stat");
    std::istringstream row(text.substr(end+1));std::string value;for(unsigned i=3;i<=22;i++)require(bool(row>>value),"uninstall-process-start");require(digits(value),"uninstall-process-start-value");return value;
}
static bool zombie(pid_t p){auto text=read("/proc/"+std::to_string(p)+"/stat",16384);auto n=text.rfind(')');require(n!=std::string::npos&&n+2<text.size(),"uninstall-process-state");return text[n+2]=='Z';}
struct Worker {pid_t pid;std::string start;};
static std::vector<Worker> workers(){
    std::vector<Worker> out;for(auto p:pids("worker")){
        if(zombie(p))continue;require(executable(p)=="/system/bin/x2d-pixel-overclock/worker","uninstall-foreign-worker");
        Parent file("/system/bin/x2d-pixel-overclock/worker");struct stat current,mapped;
        require(file.state(&current)!="ABSENT"&&!stat(("/proc/"+std::to_string(p)+"/exe").c_str(),&mapped)&&current.st_dev==mapped.st_dev&&current.st_ino==mapped.st_ino,"uninstall-worker-file-changed");
        out.push_back({p,identity(p)});
    }return out;
}
static void no_consumers(){for(const auto*name:{"render-probe","render-memory","render25","render16","factory-observer","private-capture","raw-stack-capture","stack-pack","finalize"})require(pids(name).empty(),"uninstall-consumers-active");}
static void idle(){require(get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("system","system_state","int32")=="1","uninstall-not-idle");}
static void live(bool value){auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.set_live_view",value?"boolean:true":"boolean:false"});require(!r.rc,"uninstall-live",r.rc);}
static bool status_flag(const std::string&s,const std::string&key){
    const auto token="\""+key+"\":";auto first=s.find(token);require(first!=std::string::npos&&s.find(token,first+token.size())==std::string::npos,"uninstall-worker-status-key");
    auto begin=first+token.size();while(begin<s.size()&&(s[begin]==' '||s[begin]=='\t'))begin++;
    auto end=s.find_first_of(",}\r\n \t",begin);const auto value=s.substr(begin,end-begin);require(value=="true"||value=="false","uninstall-worker-status-value");return value=="true";
}
static void mode_restore(bool&was_worker){
    auto current=workers();was_worker=!current.empty();
    if(was_worker){require(!status_flag(read("/tmp/x2d-pixel-overclock/status.json"),"busy"),"uninstall-worker-busy");write_replace("/tmp/x2d-pixel-overclock/arm.txt","DISARMED\n");}
    unsigned attempts=0;for(;;){bool preparing=false;if(exists("/tmp/x2d-pixel-overclock/status.json")){auto status=read("/tmp/x2d-pixel-overclock/status.json");preparing=status_flag(status,"preparing")||status_flag(status,"prepared");}
        if(!exists("/tmp/x2d-multishot-active")&&!preparing)break;require(++attempts<100,"uninstall-mode-restore-timeout");sleep_ms(100);
    }
    no_consumers();
    if(pids("camera-storage").empty())service("camera-storage",true);
    for(unsigned i=0;;i++){bool ready=false;try{ready=get("storage","storage_status","int32")=="0";}catch(const Error&){}if(ready)break;require(i<99,"uninstall-storage-ready-timeout");sleep_ms(100);}
}
static void remount(bool ro){
    std::istringstream rows(read("/proc/self/mounts",1024*1024));std::string line;unsigned count=0;unsigned long flags=MS_REMOUNT|(ro?MS_RDONLY:0);
    while(std::getline(rows,line)){std::istringstream row(line);std::string src,dst,type,opts;require(bool(row>>src>>dst>>type>>opts),"uninstall-mount-record");if(dst!="/system")continue;++count;
        std::istringstream options(opts);std::string option;while(std::getline(options,option,',')){
            if(option=="nosuid")flags|=MS_NOSUID;else if(option=="nodev")flags|=MS_NODEV;else if(option=="noexec")flags|=MS_NOEXEC;else if(option=="sync")flags|=MS_SYNCHRONOUS;else if(option=="dirsync")flags|=MS_DIRSYNC;else if(option=="noatime")flags|=MS_NOATIME;else if(option=="nodiratime")flags|=MS_NODIRATIME;else if(option=="relatime")flags|=MS_RELATIME;else if(option=="strictatime")flags|=MS_STRICTATIME;
        }
    }require(count==1,"uninstall-system-mount-ambiguous");require(!system_remount(flags),"uninstall-remount",90);
    std::istringstream verified(read("/proc/self/mounts",1024*1024));bool found=false;while(std::getline(verified,line)){std::istringstream row(line);std::string a,b,c,d;row>>a>>b>>c>>d;if(b=="/system"){require((","+d+",").find(ro?",ro,":",rw,")!=std::string::npos,"uninstall-remount-verify",90);found=true;}}require(found,"uninstall-mount-lost",90);
}
static void verify_guards(const std::string&stage){
    require(sha256(stage+"/guards.txt")==setup::legacy_guards_sha,"uninstall-guards-table",3);
    // 机内控制表仅核对兼容来源，检查范围与摘要直接使用编译固定表。
    for(const auto&guard:setup::legacy_guards){Parent file(guard.path);require(file.state()==guard.digest,"uninstall-firmware-guard",3);}
}
struct Entry {const setup::LegacyTarget*target;std::string before;struct stat old_inode{},new_inode{},next_inode{};bool moved=false,installed=false,next_owned=false;};
static void exact_inode(const Parent&p,const struct stat&owner){struct stat st;require(p.fd>=0&&!fstatat(p.fd,p.name.c_str(),&st,AT_SYMLINK_NOFOLLOW)&&st.st_dev==owner.st_dev&&st.st_ino==owner.st_ino&&S_ISREG(st.st_mode)&&st.st_nlink==1,"uninstall-file-ownership",90);}
static void remove_owned(const Parent&p,const struct stat&owner){exact_inode(p,owner);require(!unlinkat(p.fd,p.name.c_str(),0),"uninstall-remove-owned",90);}
static void service_ready(){require(!pids("camera-gui").empty()&&!pids("camera-service").empty(),"uninstall-power-services-missing");idle();}
static std::string status_session(const std::string&s){
    const std::string token="\"session\":\"";auto start=s.find(token);require(start!=std::string::npos&&s.find(token,start+token.size())==std::string::npos,"uninstall-worker-session");start+=token.size();
    require(start+8<s.size()&&s[start+8]=='\"'&&hex(s.substr(start,8),8),"uninstall-worker-session-value");return s.substr(start,8);
}
static std::string restart_worker(const std::string&stage,Worker&restored){
    require(workers().empty(),"uninstall-worker-restart-conflict",90);Parent program("/system/bin/x2d-pixel-overclock/worker");require(program.state()!="ABSENT","uninstall-worker-restore-missing",90);
    std::string previous_session;if(exists("/tmp/x2d-pixel-overclock/status.json"))previous_session=status_session(read("/tmp/x2d-pixel-overclock/status.json"));
    int log=open((stage+"/worker-restore.log").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);require(log>=0,"uninstall-worker-restart-log",90);
    pid_t child=fork();if(child<0){close(log);throw Error("uninstall-worker-restart-fork",90);}
    if(!child){signal(SIGHUP,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);int copy=fcntl(log,F_DUPFD_CLOEXEC,3);if(copy<0||dup2(copy,1)<0||dup2(copy,2)<0)_exit(126);close(copy);if(log!=1&&log!=2)close(log);char*argv[]={const_cast<char*>("/system/bin/x2d-pixel-overclock/worker"),nullptr};execve(argv[0],argv,environ);_exit(127);}close(log);
    restored.pid=child;const auto start=identity(child);restored.start=start;unsigned stable=0;std::string new_session;
    for(unsigned i=0;;i++){
        int status;pid_t done=waitpid(child,&status,WNOHANG);
        // 只有确实回收了本次直属子进程才清除所有权；未知 ECHILD 仍保守处理。
        if(done==child)restored.pid=0;
        require(done==0||(done<0&&errno==EINTR),"uninstall-worker-restart-exited",90);
        require(identity(child)==start,"uninstall-worker-restart-identity",90);
        bool ready=false;
        if(executable(child)=="/system/bin/x2d-pixel-overclock/worker"&&exists("/tmp/x2d-pixel-overclock/status.json")){
            auto state=read("/tmp/x2d-pixel-overclock/status.json");auto session=status_session(state);
            // 本次卸载仍持有任务锁，worker 按契约必须暂时 ready=false。
            ready=session!=previous_session&&!status_flag(state,"ready")&&!status_flag(state,"busy")&&!status_flag(state,"preparing")&&!status_flag(state,"prepared");
            if(ready){if(new_session.empty())new_session=session;require(session==new_session,"uninstall-worker-restarted-during-check",90);}
        }
        stable=ready?stable+1:0;if(stable>=21)break;require(i<69,"uninstall-worker-restart-unconfirmed",90);sleep_ms(100);
    }return new_session;
}
static bool stop_failed_restart(Worker&worker){
    // 仅用于仍持有本次任务锁期间。直属子进程未 wait 回收前 PID 不会复用。
    if(worker.pid<=0)return true;int status;pid_t result=waitpid(worker.pid,&status,WNOHANG);
    if(result==worker.pid){worker.pid=0;return true;}if(result<0&&errno!=EINTR)return false;
    if(kill(worker.pid,SIGTERM)&&errno!=ESRCH)return false;
    const auto end=monotonic_ms()+2000;
    do{result=waitpid(worker.pid,&status,WNOHANG);if(result==worker.pid){worker.pid=0;return true;}if(result<0&&errno!=EINTR)return false;sleep_ms(20);}while(monotonic_ms()<end);
    // worker 可能需要排空引用，不能采用独立 HEIF 的 KILL 策略；保留任务锁。
    return false;
}
// /proc entries can disappear after enumeration or after another member of
// the flash worker/guardian pair exits. Preserve denial and I/O errors; only
// explicit process disappearance is a successful absence observation.
static bool flash_identity(pid_t pid,std::string&start){
    const auto path="/proc/"+std::to_string(pid)+"/stat";
    int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    if(fd<0){const int cause=errno;if(cause==ENOENT||cause==ESRCH)return false;throw Error("uninstall-flash-stat-open",1,false,cause);}
    std::string text;char buffer[4096];
    try{
        struct stat st;require(!fstat(fd,&st)&&S_ISREG(st.st_mode),"uninstall-flash-stat-type");
        for(;;){ssize_t n=::read(fd,buffer,sizeof buffer);if(n<0&&errno==EINTR)continue;
            if(n<0){const int cause=errno;if(cause==ENOENT||cause==ESRCH){close(fd);return false;}throw Error("uninstall-flash-stat-read",1,false,cause);}
            if(!n)break;require(text.size()+size_t(n)<=16384,"uninstall-flash-stat-limit");text.append(buffer,size_t(n));}
    }catch(...){close(fd);throw;}close(fd);
    auto end=text.rfind(')');require(end!=std::string::npos,"uninstall-flash-stat-format");
    std::istringstream row(text.substr(end+1));std::string state,value;
    require(bool(row>>state)&&state.size()==1,"uninstall-flash-stat-state");
    for(unsigned i=4;i<=22;i++)require(bool(row>>value),"uninstall-flash-stat-start");
    require(digits(value),"uninstall-flash-stat-start-value");start=value;return state!="Z";
}
static bool flash_executable(pid_t pid){
    const auto path="/proc/"+std::to_string(pid)+"/exe";char buffer[4096];
    ssize_t n=readlink(path.c_str(),buffer,sizeof buffer);
    if(n<0){const int cause=errno;if(cause==ENOENT||cause==ESRCH)return false;throw Error("uninstall-flash-exe-read",1,false,cause);}
    require(size_t(n)<sizeof buffer&&std::string(buffer,size_t(n))=="/system/bin/x2d-esflash","uninstall-flash-executable");return true;
}
static void stop_esflash(const std::vector<Entry>&entries){
    const Entry*owned=nullptr;for(const auto&e:entries)if(std::string(e.target->path)=="/system/bin/x2d-esflash")owned=&e;
    auto list=pids("x2d-esflash");for(auto pid:list){
        std::string started;if(!flash_identity(pid,started))continue;require(owned&&owned->before!="ABSENT","uninstall-flash-unowned");
        if(!flash_executable(pid))continue;
        Parent file("/system/bin/x2d-esflash");struct stat current,mapped;require(file.state(&current)==owned->before,"uninstall-flash-file-changed");
        if(stat(("/proc/"+std::to_string(pid)+"/exe").c_str(),&mapped)){const int cause=errno;if(cause==ENOENT||cause==ESRCH)continue;throw Error("uninstall-flash-exe-stat",1,false,cause);}
        require(current.st_dev==mapped.st_dev&&current.st_ino==mapped.st_ino,"uninstall-flash-file-changed");
        std::string checked;if(!flash_identity(pid,checked))continue;require(checked==started,"uninstall-flash-reused");if(!flash_executable(pid))continue;
        require(!kill(pid,SIGTERM)||errno==ESRCH,"uninstall-flash-term");
    }
    for(unsigned i=0;;i++){bool live=false;for(auto pid:pids("x2d-esflash")){std::string start;if(flash_identity(pid,start))live=true;}if(!live)break;require(i<49,"uninstall-flash-stop-timeout");sleep_ms(100);}
}
static int run(const std::string&s,const std::string&attempt){
    const std::string prefix="/data/local/tmp/x2d-un-";require(s.rfind(prefix,0)==0&&hex(s.substr(prefix.size()),8)&&hex(attempt,16),"uninstall-arguments",2);
    const auto suffix=s.substr(prefix.size()),retained="/system/x2d-un-retained-"+suffix,backup=s+"/backup";
#ifdef CN_PACKAGED_PAYLOAD
    const std::string payload=s+"/payload";
    Parent payload_parent(payload);require(directory(payload_parent.bound()),"uninstall-payload-directory");
#else
    const std::string payload=s;
#endif
    Parent stage_parent(s);require(directory(stage_parent.bound()),"uninstall-stage-directory");Parent retained_parent(retained);
    require(!exists(retained_parent.bound())&&!exists(backup)&&!exists(s+"/started")&&!exists(s+"/result")&&!exists(s+"/done")&&!exists(s+"/final"),"uninstall-stage-reused",3);
    require(sha256(payload+"/targets.txt")==setup::legacy_targets_sha,"uninstall-target-table",3);verify_guards(payload);
    std::vector<Entry> entries;entries.reserve(sizeof(setup::legacy_targets)/sizeof(setup::legacy_targets[0]));
    for(const auto&t:setup::legacy_targets){Parent file(t.path,true);Entry item{&t,"",{},{},{},false,false,false};item.before=file.state(&item.old_inode);const std::string allowed=t.allowed;
        require(allowed.find(","+item.before+",")!=std::string::npos||(allowed.find(",PROJECT_FILE,")!=std::string::npos&&(item.before=="ABSENT"||hex(item.before,64))),"uninstall-unsupported-component",3);
        for(const auto*extension:{".xu-next",".xu-rollback"}){Parent pending(std::string(t.path)+extension,true);require(pending.state()=="ABSENT","uninstall-pending-conflict",3);}
        if(std::string(t.original)!="ABSENT"){Parent source(payload+"/stock-"+t.id);require(source.state()==t.original,"uninstall-stock-source",3);}entries.push_back(item);
    }
    // 此后开始记录本次事务，所有退出均保留独立原错/恢复结果。
    write_new(s+"/started","NATIVE_UNINSTALL_V1 "+attempt+"\n");
    std::string phase="MODE_RESTORE",primary_step="none",live_before,restored_session;Worker restored_worker{0,{}};int primary=0,recovery=0;bool paused=false,lock_owned=false,lock_known=false,worker_before=false,workers_stopped=false,switched=false,complete=false;struct stat lock{};
    try{
        require(get("camera","exposure_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("system","system_state","int32")=="1","uninstall-camera-not-idle");
        mode_restore(worker_before);idle();no_consumers();live_before=get("camera","live_view_state","int32");require(live_before=="0"||live_before=="1","uninstall-live-before");
        if(live_before=="1"){paused=true;live(false);}
        for(unsigned i=0;;i++){idle();if(get("camera","stream_group","int32")=="255")break;require(i<49,"uninstall-stream-drain-timeout");sleep_ms(100);}
        mkdir_new("/tmp/x2d-multishot-active");lock_owned=true;require(!lstat("/tmp/x2d-multishot-active",&lock)&&S_ISDIR(lock.st_mode),"uninstall-lock-identity",90);lock_known=true;
        phase="BACKUP";mkdir_new(backup);write_new(s+"/boot.sha256",sha256("/proc/sys/kernel/random/boot_id")+"\n");std::string retained_records;
        for(auto&e:entries){Parent file(e.target->path,true);struct stat current;require(file.state(&current)==e.before,"uninstall-changed-before-backup",3);if(e.before!="ABSENT")require(current.st_dev==e.old_inode.st_dev&&current.st_ino==e.old_inode.st_ino,"uninstall-identity-before-backup",3);
            write_new(backup+"/"+e.target->id+".sha",e.before+"\n");if(e.before!="ABSENT"){copy_new(file.bound(),backup+"/"+e.target->id);require(sha256(backup+"/"+e.target->id)==e.before,"uninstall-backup-verify");retained_records+=std::string(e.target->id)+" "+e.before+"\n";}}
        write_new(s+"/retained.txt",retained_records);phase="STOPPING_FLASH";stop_esflash(entries);phase="STOPPING_WORKERS";auto current_workers=workers();workers_stopped=true;
        for(const auto&w:current_workers){require(identity(w.pid)==w.start&&executable(w.pid)=="/system/bin/x2d-pixel-overclock/worker","uninstall-worker-changed-before-term");require(!kill(w.pid,SIGTERM)||errno==ESRCH,"uninstall-worker-term");}
        for(unsigned i=0;;i++){if(workers().empty())break;require(i<49,"uninstall-worker-stop-timeout");sleep_ms(100);}service_ready();
        for(auto&e:entries){Parent file(e.target->path,true);require(file.state()==e.before,"uninstall-target-concurrently-changed",3);if(e.before!="ABSENT")exact_inode(file,e.old_inode);}
        phase="RETAINING_LOADED_FILES";remount(false);switched=true;mkdir_new(retained);
        for(auto&e:entries){cancelled();Parent file(e.target->path,true);require(file.state()==e.before,"uninstall-target-changed-at-switch",3);
            if(e.before!="ABSENT"){exact_inode(file,e.old_inode);move_new(file.bound(),retained+"/"+e.target->id,false);e.moved=true;Parent kept(retained+"/"+e.target->id);exact_inode(kept,e.old_inode);require(kept.state()==e.before,"uninstall-retained-content");}
            if(std::string(e.target->original)!="ABSENT"){
                phase="RESTORING_STOCK";Parent next(std::string(e.target->path)+".xu-next");copy_new(payload+"/stock-"+e.target->id,next.bound());e.next_owned=true;require(next.state(&e.next_inode)==e.target->original,"uninstall-stock-next-content");require(!fchmodat(next.fd,next.name.c_str(),e.target->mode,0),"uninstall-stock-mode");
                move_new(next.bound(),file.bound(),false);e.installed=true;e.new_inode=e.next_inode;e.next_owned=false;require(file.state()==e.target->original,"uninstall-stock-verify");
            }
        }
        phase="VERIFYING_STOCK";for(auto&e:entries){Parent file(e.target->path,true);require(file.state()==e.target->original,"uninstall-final-target");}verify_guards(payload);remount(true);service_ready();require(workers().empty(),"uninstall-worker-returned");complete=true;
    }catch(const Error&e){primary=e.rc;primary_step=e.what();if(e.recovery_failed)recovery=90;fprintf(stderr,"UNINSTALL_PRIMARY phase=%s step=%s rc=%d\n",phase.c_str(),e.what(),primary);}
    catch(const std::exception&e){primary=1;primary_step="native-exception";fprintf(stderr,"UNINSTALL_PRIMARY phase=%s detail=%s rc=1\n",phase.c_str(),e.what());}
    {
        Recovery finish;
        if(!complete&&switched){
            try{
                remount(false);
                for(auto i=entries.rbegin();i!=entries.rend();++i){auto&e=*i;Parent file(e.target->path,true);
                    if(e.installed){require(file.state()==e.target->original,"uninstall-rollback-stock-changed",90);remove_owned(file,e.new_inode);e.installed=false;}
                    if(e.moved){require(file.state()=="ABSENT","uninstall-rollback-target-occupied",90);Parent kept(retained+"/"+e.target->id);exact_inode(kept,e.old_inode);require(kept.state()==e.before,"uninstall-rollback-retained-changed",90);move_new(kept.bound(),file.bound(),false);e.moved=false;exact_inode(file,e.old_inode);}
                }
            }catch(const std::exception&e){recovery=90;fprintf(stderr,"UNINSTALL_ROLLBACK_UNCONFIRMED detail=%s\n",e.what());}
        }
        for(auto&e:entries)if(e.next_owned){try{Parent next(std::string(e.target->path)+".xu-next");remove_owned(next,e.next_inode);}catch(const std::exception&error){recovery=90;fprintf(stderr,"UNINSTALL_TEMPORARY_UNCONFIRMED detail=%s\n",error.what());}}
        try{remount(true);}catch(const std::exception&e){recovery=90;fprintf(stderr,"UNINSTALL_READONLY_UNCONFIRMED detail=%s\n",e.what());}
        if(!complete&&!recovery&&workers_stopped&&worker_before)try{restored_session=restart_worker(s,restored_worker);service_ready();}catch(const std::exception&e){recovery=90;fprintf(stderr,"UNINSTALL_WORKER_RESTORE_UNCONFIRMED detail=%s\n",e.what());}
        if(paused&&!recovery)try{live(true);require(get("camera","live_view_state","int32")=="1","uninstall-live-restore-verify",90);}catch(const std::exception&e){recovery=90;fprintf(stderr,"UNINSTALL_LIVE_RESTORE_UNCONFIRMED detail=%s\n",e.what());}
        bool keep_lock=false;
        if(recovery&&restored_worker.pid>0)try{require(lock_owned&&lock_known,"uninstall-worker-lock-unconfirmed",90);keep_lock=!stop_failed_restart(restored_worker);if(keep_lock)fprintf(stderr,"UNINSTALL_LOCK_RETAINED_WORKER_UNCONFIRMED\n");}catch(const std::exception&e){keep_lock=true;fprintf(stderr,"UNINSTALL_WORKER_REAP_UNCONFIRMED detail=%s\n",e.what());}
        if(lock_owned&&!keep_lock)try{struct stat current;require(lock_known&&!lstat("/tmp/x2d-multishot-active",&current)&&current.st_dev==lock.st_dev&&current.st_ino==lock.st_ino,"uninstall-lock-changed",90);require(!rmdir("/tmp/x2d-multishot-active"),"uninstall-unlock",90);}catch(const std::exception&e){recovery=90;fprintf(stderr,"UNINSTALL_UNLOCK_UNCONFIRMED detail=%s\n",e.what());}
        if(!recovery&&restored_worker.pid>0)try{
            for(unsigned i=0;;i++){
                require(identity(restored_worker.pid)==restored_worker.start&&executable(restored_worker.pid)=="/system/bin/x2d-pixel-overclock/worker"&&!zombie(restored_worker.pid),"uninstall-worker-late-exit",90);
                auto state=read("/tmp/x2d-pixel-overclock/status.json");require(status_session(state)==restored_session,"uninstall-worker-session-changed",90);
                if(status_flag(state,"ready")&&!status_flag(state,"busy")&&!status_flag(state,"preparing")&&!status_flag(state,"prepared"))break;
                require(i<49,"uninstall-worker-final-ready-timeout",90);sleep_ms(100);
            }
        }catch(const std::exception&e){recovery=90;fprintf(stderr,"UNINSTALL_WORKER_FINAL_UNCONFIRMED detail=%s\n",e.what());}
        if(!primary&&cn_signal_pending()){primary=143;primary_step="cancelled-during-finish";}
        if(recovery&&switched)for(const auto*service_name:{"camera-gui","camera-service"})try{service(service_name,false);}catch(const std::exception&e){fprintf(stderr,"UNINSTALL_SERVICE_STOP_UNCONFIRMED detail=%s\n",e.what());}
        try{
            if(primary){write_new(s+"/fault.step",phase+"\n"+primary_step+"\n");write_new(s+"/fault.rc",std::to_string(primary)+"\n");}
            write_new(s+"/result",std::to_string(recovery?recovery:primary)+"\n");
            if(complete&&!primary&&!recovery)write_new(s+"/done","PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED\n");
            write_replace(s+"/final","NATIVE_TRANSACTION_V1\noperation=uninstall\ntransaction="+suffix+"\nattempt="+attempt+"\nprimary_step="+primary_step+"\nprimary_rc="+std::to_string(primary)+"\nrestore_rc="+std::to_string(recovery)+"\nfinal_rc="+std::to_string(recovery?recovery:primary)+"\nmanual_reboot_required="+(complete?"1\n":"0\n"));
        }catch(const std::exception&e){recovery=90;fprintf(stderr,"UNINSTALL_RECEIPT_UNCONFIRMED detail=%s\n",e.what());}
    }
    fprintf(stderr,"UNINSTALL_FINISH primary_rc=%d restore_rc=%d complete=%d\n",primary,recovery,complete);return recovery?recovery:primary;
}
} }
extern "C" int cn_uninstall_main(int argc,char**argv){
    try{cn::require(argc==4&&std::string(argv[1])=="uninstall","uninstall-arguments",2);cn::cancelled();cn::TransactionGuard transaction;return cn::uninstall::run(argv[2],argv[3]);}
    catch(const cn::Error&e){fprintf(stderr,"UNINSTALL_REFUSED step=%s rc=%d\n",e.what(),e.rc);return e.recovery_failed?90:e.rc;}
    catch(const std::exception&e){fprintf(stderr,"UNINSTALL_REFUSED detail=%s rc=1\n",e.what());return 1;}
}
