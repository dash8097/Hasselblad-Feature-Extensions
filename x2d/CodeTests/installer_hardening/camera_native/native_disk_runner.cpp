/* 固定 H3 p104 十六帧主流程。所有恢复失败保留任务锁和原始诊断。 */
#include "native_disk_runner.hpp"
#include "native_common.hpp"
#include "native_runtime_tables.hpp"
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <functional>
#include <array>
#include <limits>
extern char **environ;
extern "C" int cn_signal_pending(void);
extern "C" int cn_counter_main(int,char**);
extern "C" int cn_render_main(int,char**);
extern "C" int cn_publish_main(int,char**);
namespace cn { namespace disk_runner {
static const char*module="/system/bin/x2d-multi-resolution/libx2d_private_capture.so";
static const char*module_sha="d29606f07e88fbb7dde30217e857737ffa51e7ba3c0a47a540e580e7a7181204";
static bool quarter_mode=false;
static const char*render25="/system/bin/x2d-multi-resolution/render25";
static const char*render25_sha="ccab2c22a8b7263a06448a3a9fe08f0817202d43004b78784caad7e9b73a0b5a";
static const char*finalize25="/system/bin/x2d-multi-resolution/finalize25";
static const char*finalize25_sha="81e74ff2a0a5c363111f1c7f53084f38f08999c4b847cc8de53c006d3e98a8b4";
static const char*pack25="/system/bin/x2d-multi-resolution/pack25";
static const char*pack25_sha="7f49110b1bfdd06b8bcd0d481954c89d83f7c03177f8af7e58d8e0ec2cfb31ae";
static void select_quarter(bool enabled){quarter_mode=enabled;module=enabled?"/system/bin/x2d-multi-resolution/libquarter_capture.so":"/system/bin/x2d-multi-resolution/libx2d_private_capture.so";module_sha=enabled?"032ad9496b99e99991838db31c4d2ff57e85621dad6b9d8ba81074b94032eb87":"d29606f07e88fbb7dde30217e857737ffa51e7ba3c0a47a540e580e7a7181204";}
static const char*storage_sha="8c086e9c2a340c2a388e92ae3de39d6d0e5ce19ac3d582626112a24fd3b97aef";
static const char*quarter_sha="25bd7a3b90768c651987d11c297f31081e45b3e75c1e7a0ec76a070e99720216";
static const char*inputs_sha=runtime::pixel_inputs_sha;
static const std::string global="/tmp/x2d-multishot-active";
struct FD{int n=-1;explicit FD(int x=-1):n(x){}~FD(){if(n>=0)close(n);}FD(const FD&)=delete;FD&operator=(const FD&)=delete;};
static void check(bool v,const std::string&step,int rc=1){if(!v)throw Error(step,rc,false,errno);}
static void cancel(){if(stopping())throw Error("disk-cancelled",71);}
static void delay(unsigned ms){try{sleep_ms(ms);}catch(const Error&e){if(e.rc==143)throw Error("disk-cancelled",71);throw;}}
static void safe_job(const std::string&s){job(s);FD current(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));check(current.n>=0,"disk-root");std::istringstream parts(s.substr(1));std::string part;while(std::getline(parts,part,'/')){require(!part.empty()&&part!="."&&part!="..","disk-parent-component",2);int next=openat(current.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);check(next>=0,"disk-parent-type");close(current.n);current.n=next;}}
static void owned_regular(const std::string&p){struct stat st;check(!lstat(p.c_str(),&st),"disk-file-stat");require(S_ISREG(st.st_mode)&&st.st_nlink==1,"disk-file-type");}
static bool exact(const std::string&p,const std::string&text){if(!exists(p))return false;owned_regular(p);return read(p,text.size()+2)==text+"\n";}
static void record(const std::string&s,const std::string&n,const std::string&v){require(n.find('/')==std::string::npos,"disk-record-name");if(exists(s+"/"+n))owned_regular(s+"/"+n);write_replace(s+"/"+n,v+"\n");}
static void new_record(const std::string&s,const std::string&n,const std::string&v){write_new(s+"/"+n,v+"\n");}
static void fixed(const std::string&p,const char*h){owned_regular(p);require(sha256(p)==h,"disk-component-digest:"+p);}
static void remove_file(const std::string&p){owned_regular(p);remove_regular(p);}
static void note(const std::string&s,const std::string&text)noexcept{try{append(s+"/finish-restore.log",text+"\n");}catch(const std::exception&e){fprintf(stderr,"DISK_DIAGNOSTIC_WRITE_FAILED detail=%s\n",e.what());}fprintf(stderr,"%s\n",text.c_str());}
struct Identity {pid_t pid=0;std::string start;char state='?';};
static pid_t number_pid(const std::string&s){require(digits(s)&&s.size()<=10,"disk-pid-record",70);auto n=std::stoull(s);require(n>0&&n<=INT_MAX,"disk-pid-range",70);return pid_t(n);}
static Identity identify(pid_t pid){require(pid>0,"disk-pid",70);auto text=read("/proc/"+std::to_string(pid)+"/stat",16384);auto at=text.rfind(')');require(at!=std::string::npos,"disk-process-stat",70);std::istringstream words(text.substr(at+1));Identity id;id.pid=pid;std::string field;for(unsigned n=3;n<=22;++n){require(bool(words>>field),"disk-process-fields",70);if(n==3){require(field.size()==1,"disk-process-state",70);id.state=field[0];}}require(digits(field),"disk-process-start",70);id.start=field;return id;}
static bool exists_process(pid_t pid){if(!kill(pid,0))return true;check(errno==ESRCH,"disk-process-presence",70);return false;}
static bool same_live(const Identity&id){if(!exists_process(id.pid))return false;auto now=identify(id.pid);require(now.start==id.start,"disk-process-reused",70);return now.state!='Z'&&now.state!='X';}
static Identity saved_process(const std::string&s,const std::string&prefix){owned_regular(s+"/"+prefix+".pid");owned_regular(s+"/"+prefix+".start");Identity id;id.pid=number_pid(value(s+"/"+prefix+".pid"));id.start=value(s+"/"+prefix+".start");require(digits(id.start),"disk-start-record",70);return id;}
static std::vector<std::string> records(const std::string&bytes){std::vector<std::string> out;for(size_t at=0;at<bytes.size();){auto end=bytes.find('\0',at);require(end!=std::string::npos,"disk-proc-record",70);out.push_back(bytes.substr(at,end-at));at=end+1;}return out;}
static bool has(const std::vector<std::string>&values,const std::string&v){for(const auto&i:values)if(i==v)return true;return false;}
static void readonly(bool remount){
    std::istringstream rows(read("/proc/self/mounts",1024*1024));std::string line;unsigned count=0;bool ro=false;unsigned long flags=MS_REMOUNT|MS_RDONLY;
    while(std::getline(rows,line)){std::istringstream row(line);std::string a,b,c,d;require(bool(row>>a>>b>>c>>d),"disk-mount-record",90);if(b!="/system")continue;++count;std::istringstream opts(d);std::string v;while(std::getline(opts,v,',')){if(v=="ro")ro=true;else if(v=="nosuid")flags|=MS_NOSUID;else if(v=="nodev")flags|=MS_NODEV;else if(v=="noexec")flags|=MS_NOEXEC;else if(v=="sync")flags|=MS_SYNCHRONOUS;else if(v=="dirsync")flags|=MS_DIRSYNC;else if(v=="noatime")flags|=MS_NOATIME;else if(v=="nodiratime")flags|=MS_NODIRATIME;else if(v=="relatime")flags|=MS_RELATIME;else if(v=="strictatime")flags|=MS_STRICTATIME;}}
    require(count==1,"disk-system-mount",90);if(remount){check(!mount(nullptr,"/system",nullptr,flags,nullptr),"disk-system-readonly",90);readonly(false);}else require(ro,"disk-system-not-readonly",90);
}
static bool idle(){return get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0"&&get("error","active_error","boolean")=="false";}
static void live(bool on){auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.set_live_view",on?"boolean:true":"boolean:false"},6000);require(!r.rc,"disk-live",r.rc);require(get("camera","live_view_state","int32")== (on?"1":"0"),"disk-live-verify",90);}
static void put(const std::string&key,const std::string&v){require(key=="drive_mode"||key=="multishot_control_mode"||key=="exposures_in_multishot_session"||key=="image_format","disk-setting-name",2);require(digits(v),"disk-setting-value",2);auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=3000","--dest=com.hasselblad.camera","/camera","org.freedesktop.DBus.Properties.Set","string:com.hasselblad.camera","string:"+key,"variant:int32:"+v},4000);require(!r.rc,"disk-setting-write:"+key,r.rc);require(get("camera",key,"int32")==v,"disk-setting-verify:"+key);}
static void pump(bool strict=false){try{get("storage","capture_storage_mode","int32",3000);}catch(const Error&e){if(strict||e.rc==71||e.rc==143)throw;}}
static void stop_session(){auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=20000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.stop_session"},21000,false);require(!r.rc,"disk-stop-session",r.rc);}
struct Settings{std::string drive,mode,frames,format;};
static void validate(const Settings&v){require(v.drive.size()==1&&v.drive[0]>='0'&&v.drive[0]<='6'&&v.mode.size()==1&&v.mode[0]>='0'&&v.mode[0]<='3'&&(v.frames=="1"||v.frames=="4"||v.frames=="6"||v.frames=="16")&&v.format.size()==1&&v.format[0]>='0'&&v.format[0]<='4',"disk-snapshot-rejected",20);}
static Settings settings(){Settings v{get("camera","drive_mode","int32"),get("camera","multishot_control_mode","int32"),get("camera","exposures_in_multishot_session","int32"),get("camera","image_format","int32")};validate(v);return v;}
static Settings snapshot(const std::string&s){for(const auto*n:{"drive","mode","frames","format"})owned_regular(s+"/"+n+".before");Settings v{value(s+"/drive.before"),value(s+"/mode.before"),value(s+"/frames.before"),value(s+"/format.before")};validate(v);return v;}
static void save(const std::string&s,const Settings&v,bool first){std::array<std::pair<const char*,std::string>,4> all={{{"drive",v.drive},{"mode",v.mode},{"frames",v.frames},{"format",v.format}}};for(const auto&i:all){if(first)new_record(s,std::string(i.first)+".before",i.second);else record(s,std::string(i.first)+".before",i.second);}}
static bool mapped_identity(const std::string&maps,const std::string&path,const struct stat&st){
    std::istringstream rows(maps);std::string row;bool found=false;
    while(std::getline(rows,row)){
        std::istringstream fields(row);std::string address,perms,offset,dev,inode,name;
        require(bool(fields>>address>>perms>>offset>>dev>>inode),"disk-maps-record",90);
        std::getline(fields,name);auto at=name.find_first_not_of(' ');if(at==std::string::npos||name.substr(at)!=path)continue;
        auto colon=dev.find(':');require(colon!=std::string::npos&&colon>0&&colon+1<dev.size()&&digits(inode)&&inode.size()<=20,"disk-maps-fields",90);
        auto maj=dev.substr(0,colon),min=dev.substr(colon+1);
        require(maj.size()<=8&&min.size()<=8&&maj.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos&&min.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"disk-maps-device",90);
        require(std::stoull(inode)==uint64_t(st.st_ino)&&std::stoull(maj,nullptr,16)==uint64_t(major(st.st_dev))&&std::stoull(min,nullptr,16)==uint64_t(minor(st.st_dev)),"disk-maps-inode",90);found=true;
    }
    return found;
}
static bool bound_mapping(const std::string&maps,const char*path,const char*sha){struct stat before,after;require(!lstat(path,&before)&&S_ISREG(before.st_mode)&&before.st_nlink==1,"disk-mapped-file-type",90);fixed(path,sha);require(!lstat(path,&after)&&before.st_dev==after.st_dev&&before.st_ino==after.st_ino&&before.st_size==after.st_size&&before.st_mtim.tv_sec==after.st_mtim.tv_sec&&before.st_mtim.tv_nsec==after.st_mtim.tv_nsec&&before.st_ctim.tv_sec==after.st_ctim.tv_sec&&before.st_ctim.tv_nsec==after.st_ctim.tv_nsec,"disk-mapped-file-changed",90);return mapped_identity(maps,path,after);}
static Identity original(){auto ps=pids("camera-storage");require(ps.size()==1,"disk-original-storage-count",22);auto id=identify(ps[0]);require(executable(id.pid)=="/system/bin/camera-storage","disk-original-storage-exe",22);auto prefix="/proc/"+std::to_string(id.pid);std::vector<std::string> loader;for(const auto&i:records(read(prefix+"/environ",131072)))if(i.rfind("LD_",0)==0)loader.push_back(i);auto maps=read(prefix+"/maps",4*1024*1024);require(maps.find(module)==std::string::npos,"disk-original-has-capture-module",22);
    if(quarter_mode)require(loader.empty(),"disk-quarter-original-loader",22);if(!loader.empty()){require(loader.size()==1&&loader[0]=="LD_PRELOAD=/system/lib64/libx2d_quarter_native.so","disk-original-loader",22);fixed("/system/lib64/libx2d_quarter_native.so",quarter_sha);require(exact("/tmp/x2d-quarter-native.status","QUARTER_NATIVE_READY")&&exact("/tmp/x2d-pixel-overclock/quarter.txt","DISARMED")&&bound_mapping(maps,"/system/lib64/libx2d_quarter_native.so",quarter_sha),"disk-quarter-native-state",22);}
    fixed("/system/bin/camera-storage",storage_sha);require(identify(id.pid).start==id.start&&pids("camera-storage")==ps,"disk-original-storage-changed",22);return id;
}
static void stable_original(){unsigned count=0;std::string key;auto end=monotonic_ms()+20000;for(;;){try{auto id=original();auto next=std::to_string(id.pid)+":"+id.start;if(next==key)++count;else{key=next;count=1;}if(count>=3&&get("storage","storage_status","int32")=="0")return;}catch(const Error&e){if(e.rc==71||e.rc==143)throw;count=0;}require(monotonic_ms()<end,"disk-original-storage-unconfirmed",90);delay(100);}}
static void storage_identity(const std::string&s,const Identity&id){require(same_live(id)&&executable(id.pid)=="/system/bin/camera-storage","disk-owned-storage-exe",90);auto prefix="/proc/"+std::to_string(id.pid);require(bound_mapping(read(prefix+"/maps",4*1024*1024),module,module_sha),"disk-owned-storage-module",90);require(has(records(read(prefix+"/environ",131072)),std::string(quarter_mode?"X2D_RAW_STACK_JOB=":"X2D_DISK_CAPTURE_JOB=")+s),"disk-owned-storage-job",90);require(same_live(id)&&executable(id.pid)=="/system/bin/camera-storage","disk-owned-storage-changed",90);}
static void stop_owned(const std::string&s){if(!exists(s+"/storage.pid"))return;auto id=saved_process(s,"storage");if(!same_live(id))return;storage_identity(s,id);check(!kill(id.pid,SIGTERM)||errno==ESRCH,"disk-storage-term",90);auto end=monotonic_ms()+5000;while(same_live(id)){require(monotonic_ms()<end,"disk-storage-exit-unconfirmed",90);delay(100);}}
struct Lock {
    std::string path;struct stat id{};bool held=false;FD directory_fd;
    explicit Lock(std::string p):path(std::move(p)){}
    void pin_created(){
        held=true;
        // exFAT assigns in-memory inode numbers. Keep the inode referenced for
        // the whole transaction instead of comparing two unpinned lookups.
        directory_fd.n=open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        check(directory_fd.n>=0&&!fstat(directory_fd.n,&id)&&S_ISDIR(id.st_mode),"disk-lock-identity",90);
    }
    void acquire(){check(!mkdir(path.c_str(),0700),"disk-lock-create");pin_created();}
    void release(){
        struct stat now,pinned;
        require(held&&directory_fd.n>=0&&!fstat(directory_fd.n,&pinned)&&
                !lstat(path.c_str(),&now)&&S_ISDIR(now.st_mode)&&
                pinned.st_dev==id.st_dev&&pinned.st_ino==id.st_ino&&
                now.st_dev==id.st_dev&&now.st_ino==id.st_ino,"disk-lock-replaced",90);
        check(!rmdir(path.c_str()),"disk-lock-release",90);
        held=false;close(directory_fd.n);directory_fd.n=-1;
    }
};
static void lock_record(const std::string&s,const Lock&lock){new_record(s,"task.lock.identity",std::to_string((unsigned long long)lock.id.st_dev)+" "+std::to_string((unsigned long long)lock.id.st_ino));}
static void release_recorded_lock(const std::string&s){owned_regular(s+"/task.lock.identity");std::istringstream in(value(s+"/task.lock.identity"));std::string dev,ino,extra;require(bool(in>>dev>>ino)&&!(in>>extra)&&digits(dev)&&digits(ino),"disk-task-lock-record",90);struct stat st;require(!lstat(global.c_str(),&st)&&S_ISDIR(st.st_mode)&&std::stoull(dev)==st.st_dev&&std::stoull(ino)==st.st_ino,"disk-task-lock-owner",90);check(!rmdir(global.c_str()),"disk-task-unlock",90);}
static int invoke(int(*fn)(int,char**),std::vector<std::string> args){std::vector<char*> pointers;for(auto&v:args)pointers.push_back(&v[0]);pointers.push_back(nullptr);return fn(int(args.size()),pointers.data());}
struct Child {
    Identity id;bool done=false;int rc=1;std::string prefix;
    bool poll(){require(id.pid>0,"disk-child-not-started");if(done)return true;int status;pid_t result=waitpid(id.pid,&status,WNOHANG);if(result<0&&errno==EINTR)return false;check(result>=0,"disk-child-wait",90);if(result==id.pid){done=true;rc=WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?128+WTERMSIG(status):1;}return done;}
};
static Child spawn(const std::string&s,const std::string&prefix,const std::string&log,const std::function<int()>&body){
    cancel();FD output(open((s+"/"+log).c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));check(output.n>=0,"disk-child-log");int gate[2];check(!socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,gate),"disk-child-gate");FD raw_input(gate[0]),raw_release(gate[1]);FD input(fcntl(raw_input.n,F_DUPFD_CLOEXEC,3)),release(fcntl(raw_release.n,F_DUPFD_CLOEXEC,3));check(input.n>=3&&release.n>=3,"disk-child-gate-duplicate");close(raw_input.n);raw_input.n=-1;close(raw_release.n);raw_release.n=-1;Child child;child.prefix=prefix;child.id.pid=fork();check(child.id.pid>=0,"disk-child-fork");
    if(!child.id.pid){close(release.n);char go=0;ssize_t n;do{n=::read(input.n,&go,1);}while(n<0&&errno==EINTR);close(input.n);if(n!=1||go!='G')_exit(125);int copy=fcntl(output.n,F_DUPFD_CLOEXEC,3);if(copy<0)_exit(126);int null=open("/dev/null",O_RDONLY|O_CLOEXEC);if(null<0||dup2(null,0)<0)_exit(126);if(null!=0)close(null);if(dup2(copy,1)<0||dup2(copy,2)<0)_exit(126);close(copy);if(output.n>2)close(output.n);int rc=1;try{rc=body();}catch(const Error&e){fprintf(stderr,"DISK_CHILD_FAILURE step=%s rc=%d\n",e.what(),e.rc);rc=e.recovery_failed?90:e.rc;}catch(const std::exception&e){fprintf(stderr,"DISK_CHILD_FAILURE detail=%s\n",e.what());}fflush(nullptr);_exit(rc);}
    try{child.id=identify(child.id.pid);new_record(s,prefix+".pid",std::to_string(child.id.pid));new_record(s,prefix+".start",child.id.start);check(send(release.n,"G",1,MSG_NOSIGNAL)==1,"disk-child-release");}
    catch(...){close(release.n);release.n=-1;Recovery recovery;auto end=monotonic_ms()+2000;while(!child.poll()&&monotonic_ms()<end)delay(20);if(!child.done)throw Error("disk-gated-child-exit-unconfirmed",90,true);throw;}return child;
}
static void publish_child_rc(const std::string&s,Child&child){if(child.done&&!exists(s+"/"+child.prefix+".rc"))new_record(s,child.prefix+".rc",std::to_string(child.rc));}
static int wait_child(const std::string&s,Child&child,bool interruptible=true,Child*watch=nullptr){while(!child.poll()){if(watch)require(!watch->poll(),"disk-watchdog-early-exit",90);if(interruptible)cancel();delay(20);}publish_child_rc(s,child);if(interruptible)cancel();return child.rc;}
static bool drain_child(const std::string&s,Child&child,unsigned timeout,bool request_cancel){if(child.id.pid<=0)return true;Recovery recovery;if(!child.poll()&&request_cancel){require(identify(child.id.pid).start==child.id.start,"disk-child-reused",90);check(!kill(child.id.pid,SIGTERM)||errno==ESRCH,"disk-child-cancel",90);}auto end=monotonic_ms()+timeout;while(!child.poll()){if(monotonic_ms()>=end)return false;delay(50);}publish_child_rc(s,child);return child.rc!=90;}
static void env_exec_storage(const std::string&s){std::vector<std::string> entries;for(char**e=environ;*e;++e){std::string v=*e;require(v.size()<16384&&entries.size()<512,"disk-storage-environment");if(v.rfind("LD_",0)&&v.rfind("X2D_",0))entries.push_back(v);}entries.push_back(quarter_mode?"X2D_RAW_STACK_ABI=1":"X2D_PRIVATE_CAPTURE_ABI=1");entries.push_back(std::string(quarter_mode?"X2D_RAW_STACK_JOB=":"X2D_DISK_CAPTURE_JOB=")+s);entries.push_back(std::string("LD_PRELOAD=")+module);std::vector<char*> env;for(auto&v:entries)env.push_back(&v[0]);env.push_back(nullptr);char*args[]={const_cast<char*>("/system/bin/camera-storage"),nullptr};signal(SIGHUP,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);execve(args[0],args,env.data());throw Error("disk-storage-exec",127,false,errno);}
static void wait_nonowned_idle(){auto end=monotonic_ms()+30000;while(get("camera","exposure_status","int32")!="0"){require(quarter_mode&&monotonic_ms()<end,"disk-nonowned-exposure",90);delay(1000);}}
static void wait_exposure_idle(const std::string&s){auto state=get("camera","exposure_status","int32");if(state!="0"){if(!exact(s+"/capture.invoked","ONCE_V1")){wait_nonowned_idle();return;}try{stop_session();}catch(const std::exception&e){note(s,std::string("STOP_SESSION_REPLY_UNCONFIRMED ")+e.what());}auto end=monotonic_ms()+30000;while(state!="0"){require(monotonic_ms()<end,"disk-own-exposure-not-idle",90);delay(1000);pump();state=get("camera","exposure_status","int32");}}}
static void source_release_receipt(const std::string&s){
    if(exists(s+"/capture.invoked"))require(exact(s+"/capture.invoked","ONCE_V1"),"disk-capture-ownership-record",90);
    bool ready=quarter_mode?(exact(s+"/raw.ready","RAW_STACK_R1")&&exact(s+"/quarter.memory.copied","QUARTER_COPIED_R1")):exists(s+"/frames.ready");
    if(ready){if(!quarter_mode)owned_regular(s+"/frames.ready");return;}
    // request is issued only after RPC completion and verified exposure/storage idle.
    if(exists(s+"/capture.pid"))require(!same_live(saved_process(s,"capture")),"disk-capture-rpc-not-drained",90);
    if(!exists(s+"/storage.pid")){require(!exists(s+"/capture.invoked"),"disk-missing-storage-identity",90);return;}
    auto storage=saved_process(s,"storage");
    if(!same_live(storage)){require(!exists(s+"/capture.invoked"),"disk-capture-storage-lost-before-drain",90);return;}
    storage_identity(s,storage);
    if(quarter_mode&&exists(s+"/quarter-render.pid"))require(!same_live(saved_process(s,"quarter-render")),"quarter-consumer-not-drained",90);
    const char*request=quarter_mode?"QUARTER_ABORT_IDLE_CONSUMER_DRAINED_V1":"SIXTEEN_ABORT_IDLE_V1";
    const char*receipt=quarter_mode?"QUARTER_ABORT_DRAINED_V1":"SIXTEEN_ABORT_DRAINED_V1";
    if(exists(s+"/abort.request"))require(exact(s+"/abort.request",request),"disk-abort-request-conflict",90);else new_record(s,"abort.request",request);
    auto end=monotonic_ms()+30000;
    for(;;){if(exact(s+"/abort.drained",receipt))return;require(same_live(storage)&&monotonic_ms()<end,"disk-source-release-unconfirmed",90);storage_identity(s,storage);pump();delay(100);}
}
static void restored_check(const std::string&s){require(exact(s+"/restored","DISK_RESTORED_V1")&&exact(s+"/restore.rc","0"),"disk-restored-receipt",90);readonly(false);fixed(module,module_sha);stable_original();if(exists(s+"/settings.modified")){auto v=snapshot(s),actual=settings();require(v.drive==actual.drive&&v.mode==actual.mode&&v.frames==actual.frames&&v.format==actual.format,"disk-restored-settings-changed",90);}}
static int restore(const std::string&s){
    Recovery recovery;require(exact(s+"/runner.mode",quarter_mode?"QUARTER_V1":"SIXTEEN_V1"),"disk-restore-mode",90);if(exists(s+"/restored")){restored_check(s);return 0;}Lock lock(s+"/restore.lock");
    for(unsigned i=0;;++i){if(!mkdir(lock.path.c_str(),0700)){lock.pin_created();break;}check(errno==EEXIST,"disk-restore-lock-create",90);require(directory(lock.path),"disk-restore-lock-type",90);if(exists(s+"/restored")){restored_check(s);return 0;}require(i<29,"disk-restore-lock-busy",90);delay(1000);}
    int primary=0,recovery_rc=0;std::string step="start";bool marker=false;
    try{if(exists(s+"/restored")){restored_check(s);lock.release();return 0;}record(s,"restore.rc","125");
        if(!exists(s+"/capture.invoked")){step="nonowned-exposure";wait_nonowned_idle();if((!exists(s+"/storage.pid")||!same_live(saved_process(s,"storage")))&&exists(s+"/storage.stopped")){owned_regular(s+"/storage.stopped");service("camera-storage",true);stable_original();}}
        step="exposure-idle";record(s,"restore.phase",step);wait_exposure_idle(s);step="storage-idle";record(s,"restore.phase",step);auto end=monotonic_ms()+30000;while(get("storage","storage_status","int32")!="0"){require(monotonic_ms()<end,"disk-storage-io-active",90);pump();delay(1000);}
        step="source-references-released";record(s,"restore.phase",step);source_release_receipt(s);
        if(exists(s+"/settings.modified")){owned_regular(s+"/settings.modified");auto v=snapshot(s);bool failed=false;std::array<std::pair<const char*,std::string>,4> values={{{"drive_mode",v.drive},{"multishot_control_mode",v.mode},{"exposures_in_multishot_session",v.frames},{"image_format",v.format}}};for(const auto&i:values){step=i.first;record(s,"restore.phase",step);try{put(i.first,i.second);}catch(const std::exception&e){failed=true;note(s,std::string("SETTINGS_RESTORE_FAILED ")+i.first+" "+e.what());}}require(!failed,"disk-settings-restore-unconfirmed",90);}
        step="owned-storage-exit";record(s,"restore.phase",step);stop_owned(s);fixed(module,module_sha);readonly(true);if(exists(s+"/storage.stopped")){step="original-storage-start";record(s,"restore.phase",step);service("camera-storage",true);stable_original();}
        step="durable-restored";sync();new_record(s,"restored","DISK_RESTORED_V1");marker=true;record(s,"restore.rc","0");record(s,"restore.phase","COMPLETE");sync();note(s,"PRIVATE_CAPTURE_ORIGINAL_SETTINGS_STORAGE_AND_READ_ONLY_RESTORED");
    }catch(const Error&e){primary=e.rc;note(s,"DISK_RESTORE_PRIMARY step="+step+" detail="+e.what()+" rc="+std::to_string(primary));}
    catch(const std::exception&e){primary=1;note(s,"DISK_RESTORE_PRIMARY step="+step+" detail="+e.what()+" rc=1");}
    try{readonly(true);}catch(const std::exception&e){recovery_rc=90;note(s,std::string("DISK_RESTORE_READONLY_UNCONFIRMED ")+e.what());}
    try{lock.release();}catch(const std::exception&e){recovery_rc=90;note(s,std::string("DISK_RESTORE_UNLOCK_UNCONFIRMED ")+e.what());}
    // Releasing the restore lock is part of recovery. Do not leave a success
    // marker behind when that final step fails.
    if((primary||recovery_rc)&&marker)try{remove_file(s+"/restored");}catch(const std::exception&e){recovery_rc=90;note(s,std::string("DISK_RESTORE_MARKER_UNCONFIRMED ")+e.what());}
    int result=recovery_rc?90:primary;try{record(s,"restore.rc",std::to_string(result));}catch(...){result=90;}return result;
}
static void scratch(){struct statvfs st;check(!statvfs("/mnt/media_rw/ssd",&st),"disk-scratch-read",19);uint64_t unit=st.f_frsize,blocks=st.f_bavail;require(unit&&blocks<=std::numeric_limits<uint64_t>::max()/unit,"disk-scratch-overflow",19);require(blocks*unit>=uint64_t(7340032)*1024,"SIXTEEN_7GIB_SCRATCH_REQUIRED",19);}
static void stable_idle(){unsigned stable=0;for(unsigned i=0;i<20;++i){cancel();stable=idle()?stable+1:0;if(stable>=5)return;delay(100);}throw Error("disk-native-idle-unstable",19);}
static bool finished_token(const std::string&s){if(!exists(s+"/run.rc")||!exists(s+"/runner.final"))return false;owned_regular(s+"/run.rc");owned_regular(s+"/runner.final");auto rc=value(s+"/run.rc"),final=value(s+"/runner.final");require(digits(rc)&&rc.size()<=3&&std::stoul(rc)<=255&&final.rfind("DISK_RUNNER_V1 primary_rc=",0)==0&&final.find(" restore_rc=")!=std::string::npos,"disk-final-receipt-invalid",90);return true;}
static void cancel_orphan(const std::string&s,const std::string&prefix,unsigned timeout){
    if(!exists(s+"/"+prefix+".pid"))return;auto id=saved_process(s,prefix);if(same_live(id)){auto expected=executable(getpid());if(prefix=="finalize"){fixed(finalize25,finalize25_sha);expected=finalize25;}else if(prefix=="pack"){fixed(pack25,pack25_sha);expected=pack25;}require(executable(id.pid)==expected,"disk-orphan-executable",90);check(!kill(id.pid,SIGTERM)||errno==ESRCH,"disk-orphan-term",90);auto end=monotonic_ms()+timeout;while(same_live(id)){require(monotonic_ms()<end,"disk-orphan-exit-unconfirmed",90);delay(100);}}
    require(exists(s+"/"+prefix+".rc"),"disk-orphan-result-unconfirmed",90);owned_regular(s+"/"+prefix+".rc");auto result=value(s+"/"+prefix+".rc");require(digits(result)&&result!="90","disk-orphan-recovery-failed",90);
}
static bool drain_quarter_orphan(const std::string&s){
    if(!exists(s+"/quarter-render.pid"))return true;auto id=saved_process(s,"quarter-render");if(same_live(id)){fixed(render25,render25_sha);require(executable(id.pid)==render25,"quarter-render-orphan-executable",90);auto args=records(read("/proc/"+std::to_string(id.pid)+"/cmdline",131072));require(has(args,"--quarter-single")&&has(args,s),"quarter-render-orphan-arguments",90);record(s,"quarter.render.abort","ABORT");auto end=monotonic_ms()+190000;while(same_live(id)){require(monotonic_ms()<end,"quarter-render-drain-unconfirmed",90);delay(100);}}
    if(!exists(s+"/quarter-render.rc"))return false;owned_regular(s+"/quarter-render.rc");auto rc=value(s+"/quarter-render.rc");return digits(rc)&&rc!="90";
}
static int watchdog(const std::string&s,pid_t owner){
    Recovery recovery;safe_job(s);require(exact(s+"/runner.mode",quarter_mode?"QUARTER_V1":"SIXTEEN_V1"),"disk-watchdog-mode",90);auto runner=saved_process(s,"runner");require(runner.pid==owner,"disk-watchdog-owner",70);auto end=monotonic_ms()+90000000;
    while(same_live(runner)&&!finished_token(s)&&monotonic_ms()<end)delay(250);if(finished_token(s))return 0;
    if(same_live(runner)){auto args=records(read("/proc/"+std::to_string(owner)+"/cmdline",131072));require(has(args,s)&&has(args,quarter_mode?"quarter-run":"sixteen-run")&&executable(owner)==executable(getpid()),"disk-watchdog-owner-command",70);check(!kill(owner,SIGTERM)||errno==ESRCH,"disk-watchdog-owner-term",70);auto drain=monotonic_ms()+35000;while(same_live(runner)&&!finished_token(s)){require(monotonic_ms()<drain,"disk-watchdog-owner-still-running",90);delay(250);}if(finished_token(s))return 0;}
    cancel_orphan(s,"counter-before",35000);cancel_orphan(s,"pipeline",35000);cancel_orphan(s,"counter-after",35000);require(!exists(s+"/counter-restore.failed"),"disk-counter-recovery-unconfirmed",90);wait_exposure_idle(s);
    cancel_orphan(s,"capture",5000);bool render_confirmed=!quarter_mode||drain_quarter_orphan(s);if(quarter_mode){cancel_orphan(s,"finalize",35000);cancel_orphan(s,"pack",35000);}int rc=restore(s);require(!rc,"disk-watchdog-restore",90);require(render_confirmed,"quarter-render-result-unconfirmed",90);if(exists(s+"/publish.started")){require(exists(s+"/publish.rc"),"disk-publication-result-unconfirmed",90);owned_regular(s+"/publish.rc");auto result=value(s+"/publish.rc");require(digits(result)&&result!="90","disk-publication-recovery-unconfirmed",90);}if(exists(s+"/live.before")&&value(s+"/live.before")=="1")live(true);release_recorded_lock(s);record(s,"run.rc","71");record(s,"runner.final","DISK_RUNNER_V1 primary_rc=71 restore_rc=0 watchdog=1");return 0;
}
static int fixed_exec(std::vector<std::string> args){
    require(!args.empty(),"disk-leaf-arguments",2);const char*digest=nullptr;if(args[0]==render25)digest=render25_sha;else if(args[0]==finalize25)digest=finalize25_sha;else if(args[0]==pack25)digest=pack25_sha;else throw Error("disk-leaf-not-allowed",2);fixed(args[0],digest);
    std::vector<std::string>entries;for(char**e=environ;*e;++e){std::string v=*e;require(v.size()<16384&&entries.size()<512,"disk-leaf-environment");if(v.rfind("LD_",0)!=0)entries.push_back(v);}std::vector<char*>env,argv;for(auto&v:entries)env.push_back(&v[0]);env.push_back(nullptr);for(auto&v:args)argv.push_back(&v[0]);argv.push_back(nullptr);signal(SIGHUP,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);execve(argv[0],argv.data(),env.data());return 127;
}
static void quarter_pipeline(const std::string&s,unsigned seconds,Child&watch,Child&storage,Child&capture,Child&renderer,Child&finalizer,Child&packer,std::string&phase){
    auto deadline=monotonic_ms()+uint64_t(seconds)*1000;auto progress=[&]{std::string count="0";if(exists(s+"/frame.done")){owned_regular(s+"/frame.done");auto token=value(s+"/frame.done");require(token=="RS1 1","quarter-frame-done-invalid",26);count="1";}auto now=monotonic_ms();record(s,"stack.progress",count+" 0 "+std::to_string(now<deadline?(deadline-now+999)/1000:0));};
    auto alive=[&]{cancel();require(!watch.poll(),"disk-watchdog-early-exit",90);require(!storage.poll()&&!exists(s+"/failed")&&get("error","active_error","boolean")=="false","quarter-capture-failed");if(capture.id.pid>0&&capture.poll()){publish_child_rc(s,capture);require(!capture.rc,"quarter-capture-request",capture.rc);}if(renderer.id.pid>0&&renderer.poll()){publish_child_rc(s,renderer);require(!renderer.rc,"quarter-render-early-failure",renderer.rc);}pump(true);};
    auto token=[&](const std::string&name,const std::string&wanted){auto until=monotonic_ms()+180000;for(;;){alive();if(exact(s+"/"+name,wanted))return;require(monotonic_ms()<until,"RAW_STACK_FRAME_WAIT_TIMED_OUT:"+name,26);progress();delay(10);}};
    phase="CAPTURE";record(s,"stage","capture");alive();require(get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0","quarter-before-exposure-not-idle",26);new_record(s,"frame.request","RS1 1");token("frame.armed","RS1 1");
    capture=spawn(s,"capture","capture-request.log",[&]{new_record(s,"capture.invoked","ONCE_V1");auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=180000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.do_exposure_extended2","boolean:true","int32:14","int32:180000"},180000,false);fwrite(r.output.data(),1,r.output.size(),stdout);new_record(s,"capture.rc",std::to_string(r.rc));return r.rc;});token("frame.accepted","RS1 1");auto until=monotonic_ms()+180000;for(;;){alive();if(capture.done&&get("camera","exposure_status","int32")=="0")break;require(monotonic_ms()<until,"quarter-exposure-idle-timeout",26);progress();delay(10);}require(!capture.rc,"quarter-exposure-request",capture.rc);progress();
    phase="MERGE";new_record(s,"finish.request","FINISH");record(s,"stage","merge");token("quarter.memory.ready","QUARTER_MEMORY_R1");renderer=spawn(s,"quarter-render","probe.log",[&]{return fixed_exec({render25,"--quarter-single",s});});token("raw.ready","RAW_STACK_R1");progress();puts("RAW_STACK_FINISHED_ALL_FRAMES_RELEASED");
    phase="RESTORE";record(s,"stage","restore");int rc=restore(s);require(!rc,"quarter-original-restore",90);phase="RENDER";record(s,"stage","render");require(exact("/tmp/x2d-pixel-render-route.status","READY"),"quarter-render-route");live(false);until=monotonic_ms()+5000;while(get("camera","stream_group","int32")!="255"){require(monotonic_ms()<until,"quarter-render-stream-not-idle",30);delay(100);}note(s,"RAW_STACK_RENDER_STREAM_IDLE_AFTER_STORAGE_RESTORE");new_record(s,"quarter.render.allow","QUARTER_ALLOW_R1");rc=wait_child(s,renderer,true,&watch);new_record(s,"render.rc",std::to_string(rc));require(!rc,"quarter-render",rc);require(get("camera","stream_group","int32")=="255"&&get("error","active_error","boolean")=="false","quarter-render-state");owned_regular(s+"/probe.log");auto probe=read(s+"/probe.log",4*1024*1024);require(("\n"+probe).find("\nRAW_STACK_NATIVE_JPEG_AND_FINAL_PREVIEW_COMPLETE\n")!=std::string::npos,"quarter-render-completion-marker");
    phase="FINALIZE";record(s,"stage","raw");finalizer=spawn(s,"finalize","finalize.log",[&]{return fixed_exec({finalize25,s});});rc=wait_child(s,finalizer,true,&watch);require(!rc,"quarter-finalize",rc);require(exact(s+"/frame.done","RS1 1"),"quarter-final-frame-done");phase="PACKAGE";record(s,"stage","package");packer=spawn(s,"pack","pack.log",[&]{return fixed_exec({pack25,s+"/render.jpg",s+"/preview.jpg",s+"/album.jpg","--direct-staged-output","--capture-raw",s+"/merged.3FR"});});rc=wait_child(s,packer,true,&watch);require(!rc,"quarter-pack",rc);
}
static int runner(const std::string&s,unsigned initial_delay,unsigned method=0,unsigned seconds=1,unsigned noise=0){
    safe_job(s);if(!quarter_mode)scratch();for(const auto*n:{"capture.log","capture.requested","capture.invoked","storage.pid","runner.pid","run.rc","runner.final","restored","abort.request","abort.drained"})require(!exists(s+"/"+n),"disk-existing-job-output");for(unsigned i=0;i<(quarter_mode?1u:16u);++i)require(!exists(s+"/frame-"+std::to_string(i)+".3FR"),"disk-existing-frame");
    fixed(module,module_sha);fixed("/system/bin/camera-storage",storage_sha);fixed("/system/bin/x2d-pixel-overclock/inputs",inputs_sha);owned_regular("/system/lib64/libQt6Core.so.6");Lock task(global);task.acquire();std::string phase="LOCK";Child plan,counter_before,counter_after,storage,capture,pipeline,watch,renderer,finalizer,packer;int primary=0,recovery_rc=0;bool state_touched=false;std::string primary_step;
    try{
        lock_record(s,task);new_record(s,"runner.mode",quarter_mode?"QUARTER_V1":"SIXTEEN_V1");mkdir_new(s+"/run.lock");auto self=identify(getpid());new_record(s,"runner.pid",std::to_string(self.pid));new_record(s,"runner.start",self.start);stable_idle();auto shutter=get("camera","eshutter_current","boolean");require(shutter=="true"||shutter=="false","disk-shutter-state",19);require(get("storage","capture_storage_mode","int32")=="1","disk-local-storage-required",19);auto depth=get("camera","image_bit_depth","int32");require(depth=="0"||depth=="1","disk-bit-depth",19);
        phase="OUTPUT_PLAN";record(s,"stage","prepare");plan=spawn(s,"plan","plan.log",[&]{std::string output=s+"/output-plan";char*args[]={const_cast<char*>("/system/bin/x2d-pixel-overclock/inputs"),const_cast<char*>("plan-output"),const_cast<char*>("/mnt/media_rw/ssd/DCIM"),&output[0],nullptr};execve(args[0],args,environ);return 127;});int rc=wait_child(s,plan);require(!rc,"disk-output-plan",rc);
        phase="COUNTER_BEFORE";counter_before=spawn(s,"counter-before","counter-before.log",[&]{check(!setenv("X2D_COUNTER_WAIT_IDLE","1",1),"disk-counter-environment");int rc=invoke(cn_counter_main,{"native","counter-sync",s,"before"});new_record(s,"counter-before.rc",std::to_string(rc));return rc;});rc=wait_child(s,counter_before);require(!rc,"disk-counter-before",rc);
        phase="USER_SETTINGS";save(s,settings(),true);original();readonly(true);watch=spawn(s,"watchdog","watchdog.log",[&,owner=self.pid]{return watchdog(s,owner);});
        phase="STORAGE_REPLACE";if(quarter_mode){fixed(render25,render25_sha);fixed(finalize25,finalize25_sha);fixed(pack25,pack25_sha);new_record(s,"stack.config","RS1 "+std::to_string(method)+" "+std::to_string(noise));}new_record(s,"storage.stopped","STOP_REQUESTED_V1");state_touched=true;service("camera-storage",false);auto until=monotonic_ms()+5000;while(!pids("camera-storage").empty()){require(monotonic_ms()<until,"disk-original-storage-stop-timeout",23);delay(100);}
        storage=spawn(s,"storage","storage-process.log",[&]{env_exec_storage(s);return 127;});until=monotonic_ms()+20000;while(!exists(s+"/hook.ready")){require(!watch.poll(),"disk-watchdog-early-exit",90);require(!storage.poll(),"disk-storage-early-exit",23);pump();require(!exists(s+"/failed"),"disk-hook-failed",23);require(monotonic_ms()<until,"disk-hook-ready-timeout",23);delay(1000);}owned_regular(s+"/hook.ready");storage_identity(s,storage.id);
        phase="MANUAL_PREPARE";new_record(s,"prepared.ready","PREPARED");until=monotonic_ms()+1800000;while(!exists(s+"/manual.trigger")){cancel();require(!watch.poll(),"disk-watchdog-early-exit",90);require(!storage.poll()&&!exists(s+"/failed"),"disk-prepared-storage-failed");require(monotonic_ms()<until,"disk-manual-prepare-timeout",29);delay(100);}require(exact(s+"/manual.trigger","GO"),"disk-manual-trigger",19);remove_file(s+"/prepared.ready");
        phase="MANUAL_SETTINGS";auto v=settings();save(s,v,false);if(exists(s+"/format-source")){owned_regular(s+"/format-source");auto prior=value(s+"/format-source");require(prior.size()==1&&prior[0]>='0'&&prior[0]<='4',"disk-format-source-initial");record(s,"format-source",v.format);}else new_record(s,"format-source",v.format);auto before=get("camera","live_view_state","int32");require(before=="0"||before=="1","disk-live-before",19);new_record(s,"live.before",before);if(before=="1")live(false);until=monotonic_ms()+5000;while(get("camera","stream_group","int32")!="255"){require(monotonic_ms()<until,"disk-stream-not-idle",19);delay(100);}
        new_record(s,"settings.modified","MODIFIED_V1");sync();put("image_format","0");if(quarter_mode){put("drive_mode","0");record(s,"stack.progress","0 0 "+std::to_string(seconds));}else{put("multishot_control_mode","3");put("exposures_in_multishot_session","16");put("drive_mode","6");new_record(s,"capture.requested","16");pump(true);require(exists(s+"/owner.ready")&&!exists(s+"/failed"),"disk-owner-not-ready",19);owned_regular(s+"/owner.ready");}
        phase="DELAY";record(s,"stage","delay");until=monotonic_ms()+uint64_t(initial_delay)*1000;while(monotonic_ms()<until){cancel_before_capture(s);require(!watch.poll(),"disk-watchdog-early-exit",90);require(!storage.poll()&&!exists(s+"/failed"),"disk-delay-storage-failed");auto now=monotonic_ms();record(s,"countdown",std::to_string(now<until?(until-now+999)/1000:0));delay(100);}record(s,"countdown","0");cancel_before_capture(s);
        if(quarter_mode){quarter_pipeline(s,seconds,watch,storage,capture,renderer,finalizer,packer,phase);}else{
        cancel_before_capture(s);phase="CAPTURE";record(s,"stage","capture");require(get("camera","exposure_status","int32")=="0"&&get("error","active_error","boolean")=="false","disk-before-exposure-not-idle",19);
        capture=spawn(s,"capture","capture-request.log",[&]{new_record(s,"capture.invoked","ONCE_V1");auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=180000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.do_exposure_extended2","boolean:true","int32:24","int32:180000"},180000,false);fwrite(r.output.data(),1,r.output.size(),stdout);new_record(s,"capture.rc",std::to_string(r.rc));return r.rc;});until=monotonic_ms()+300000;
        for(;;){cancel();require(!watch.poll(),"disk-watchdog-early-exit",90);require(!storage.poll(),"disk-capture-storage-exit");pump(true);require(!exists(s+"/failed")&&get("error","active_error","boolean")=="false","disk-capture-failed");if(capture.poll()){publish_child_rc(s,capture);require(!capture.rc,"disk-exposure-request",capture.rc);}if(exists(s+"/frames.ready")&&capture.done&&get("camera","exposure_status","int32")=="0"){owned_regular(s+"/frames.ready");break;}require(monotonic_ms()<until,"SIXTEEN_CAPTURE_TIMEOUT",26);delay(100);}
        phase="RESTORE";record(s,"stage","restore");rc=restore(s);require(!rc,"disk-original-restore",90);record(s,"stage","render");require(exact("/tmp/x2d-pixel-render-route.status","READY"),"disk-render-route");live(false);until=monotonic_ms()+5000;while(get("camera","stream_group","int32")!="255"){require(monotonic_ms()<until,"disk-render-stream-not-idle",30);delay(100);}note(s,"RAW_STACK_RENDER_STREAM_IDLE_AFTER_STORAGE_RESTORE");
        phase="PIPELINE";pipeline=spawn(s,"pipeline","render.log",[&]{int rc=invoke(cn_render_main,{"native","render-sixteen",s});new_record(s,"pipeline.rc",std::to_string(rc));return rc;});rc=wait_child(s,pipeline,true,&watch);require(!rc,"disk-sixteen-pipeline",rc);require(exact(s+"/sixteen.output.ready","SIXTEEN_RAW_AND_JPEG_DURABLE"),"disk-sixteen-output-receipt");new_record(s,"raw.ready","RAW_STACK_R1");}record(s,"format-source","0");
        phase="PUBLISH";record(s,"stage","publish");require(!watch.poll(),"disk-watchdog-early-exit",90);new_record(s,"publish.started","DISK_PUBLISH_V1");rc=invoke(cn_publish_main,{"native",quarter_mode?"publish-quarter":"publish-stack",s,"0"});new_record(s,"publish.rc",std::to_string(rc));require(!rc,"disk-publish",rc);counter_after=spawn(s,"counter-after","counter-after.log",[&]{int rc=invoke(cn_counter_main,{"native","counter-sync",s,"after"});new_record(s,"counter-after.rc",std::to_string(rc));return rc;});rc=wait_child(s,counter_after,true,&watch);require(!rc,"disk-counter-after",rc);owned_regular(s+"/album-location");
        for(const auto*n:{"render.jpg","rawpb.bin","preview.jpg","album.jpg"})if(exists(s+"/"+n))remove_file(s+"/"+n);if(quarter_mode)for(const auto*n:{"source.header","raw.memory","frame.request","frame.armed","frame.done","finish.request"})if(exists(s+"/"+n))remove_file(s+"/"+n);puts("RAW_STACK_FINAL_RAW_AND_SELECTED_COMPRESSED_IMAGE_COMPLETE");
    }catch(const Error&e){primary=e.rc;primary_step=e.what();if(e.recovery_failed||e.rc==90)recovery_rc=90;fprintf(stderr,"DISK_RUNNER_PRIMARY phase=%s step=%s rc=%d\n",phase.c_str(),e.what(),primary);}
    catch(const std::exception&e){primary=1;primary_step=e.what();fprintf(stderr,"DISK_RUNNER_PRIMARY phase=%s detail=%s\n",phase.c_str(),e.what());}
    {Recovery recovery;
        try{if(primary)record(s,"failure.result","exit="+std::to_string(primary)+"\nstage="+(exists(s+"/stage")?value(s+"/stage"):"prepare")+"\nphase="+phase+"\nstep="+primary_step);}catch(...){recovery_rc=90;}
        for(auto*child:{&plan,&counter_before,&pipeline,&counter_after,&finalizer,&packer})try{if(!drain_child(s,*child,35000,primary!=0))recovery_rc=90;}catch(const std::exception&e){recovery_rc=90;note(s,std::string("CHILD_DRAIN_UNCONFIRMED ")+e.what());}
        if(exists(s+"/counter-restore.failed"))recovery_rc=90;
        try{if(capture.id.pid>0){if(!capture.poll())wait_exposure_idle(s);if(!drain_child(s,capture,190000,false))recovery_rc=90;}}catch(const std::exception&e){recovery_rc=90;note(s,std::string("CAPTURE_RPC_DRAIN_UNCONFIRMED ")+e.what());}
        bool render_drained=true;if(renderer.id.pid>0)try{if(!renderer.poll())record(s,"quarter.render.abort","ABORT");render_drained=drain_child(s,renderer,190000,false);if(!render_drained)recovery_rc=90;}catch(const std::exception&e){render_drained=false;recovery_rc=90;note(s,std::string("QUARTER_RENDER_DRAIN_UNCONFIRMED ")+e.what());}
        if(state_touched&&render_drained)try{int rc=restore(s);note(s,"SIXTEEN_RESTORE_RESULT rc="+std::to_string(rc));if(rc)recovery_rc=90;}catch(const std::exception&e){recovery_rc=90;note(s,std::string("SIXTEEN_RESTORE_UNCONFIRMED ")+e.what());}
        if(storage.id.pid>0&&render_drained)try{if(!drain_child(s,storage,5000,false))recovery_rc=90;}catch(const std::exception&e){recovery_rc=90;note(s,std::string("STORAGE_CHILD_REAP_UNCONFIRMED ")+e.what());}
        if(!recovery_rc)try{if(exists(s+"/live.before")&&value(s+"/live.before")=="1")live(true);if(quarter_mode)new_record(s,"lock.releasable","DISK_RESTORED_V1");task.release();}catch(const std::exception&e){recovery_rc=90;note(s,std::string("SIXTEEN_LOCK_OR_LIVE_UNCONFIRMED ")+e.what());}
        if(!primary&&cn_signal_pending()){primary=71;primary_step="cancelled-during-finish";}
        try{record(s,"run.rc",std::to_string(recovery_rc?90:primary));record(s,"runner.final","DISK_RUNNER_V1 primary_rc="+std::to_string(primary)+" restore_rc="+std::to_string(recovery_rc)+" phase="+phase+" step="+primary_step);}catch(...){recovery_rc=90;}
        if(watch.id.pid>0)try{if(!drain_child(s,watch,2000,false))note(s,"WATCHDOG_EXIT_UNCONFIRMED");}catch(...){note(s,"WATCHDOG_EXIT_UNCONFIRMED");}
    }return recovery_rc?90:primary;
}
} }
extern "C" int cn_disk_runner_main(int argc,char**argv){
    try{cn::require(argc>=3,"disk-arguments",2);std::string mode=argv[1],s=argv[2];cn::disk_runner::safe_job(s);bool quarter=mode=="quarter-run"||mode=="quarter-restore"||mode=="quarter-watchdog";cn::disk_runner::select_quarter(quarter);
        if(mode=="sixteen-restore"||mode=="quarter-restore"){cn::require(argc==3,"disk-restore-arguments",2);return cn::disk_runner::restore(s);}
        if(mode=="sixteen-watchdog"||mode=="quarter-watchdog"){cn::require(argc==4,"disk-watchdog-arguments",2);return cn::disk_runner::watchdog(s,cn::disk_runner::number_pid(argv[3]));}
        if(quarter){cn::require(mode=="quarter-run"&&argc==8&&std::string(argv[7])=="--prepare","quarter-run-arguments",2);std::string method=argv[3],seconds=argv[4],noise=argv[5],delay=argv[6];cn::require((method=="0"&&(noise=="0"||noise=="1"))||(method=="1"&&(noise=="0"||noise=="1"))||(method=="2"&&noise=="0"),"quarter-method-noise",2);cn::require(cn::digits(seconds)&&seconds.size()<=5&&seconds[0]!='0'&&std::stoul(seconds)<=86399,"quarter-seconds",2);cn::require(cn::digits(delay)&&delay.size()<=2&&(delay=="0"||delay[0]!='0')&&std::stoul(delay)<=60,"quarter-delay",2);return cn::disk_runner::runner(s,unsigned(std::stoul(delay)),unsigned(std::stoul(method)),unsigned(std::stoul(seconds)),unsigned(std::stoul(noise)));}
        cn::require(mode=="sixteen-run"&&argc==6&&std::string(argv[4])=="16"&&std::string(argv[5])=="--prepare","disk-run-arguments",2);std::string n=argv[3];cn::require(cn::digits(n)&&n.size()<=2&&n[0]!='0',"disk-delay",2);auto delay=std::stoul(n);cn::require(delay>=2&&delay<=60,"disk-delay-range",2);return cn::disk_runner::runner(s,unsigned(delay));
    }catch(const cn::Error&e){fprintf(stderr,"DISK_RUNNER_REFUSED step=%s rc=%d\n",e.what(),e.rc);return e.recovery_failed?90:e.rc;}catch(const std::exception&e){fprintf(stderr,"DISK_RUNNER_REFUSED detail=%s\n",e.what());return 1;}
}
