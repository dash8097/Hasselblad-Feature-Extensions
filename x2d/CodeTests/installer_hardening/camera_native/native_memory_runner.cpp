/* p118 内存六帧主流程的原生迁移候选。禁止 shell/system/popen。
 * 精确基线：H3 v4 p118 3908f16335a4ac1ecefb7e54a78122b42a1491232069dc843a38a5c3f4920beb。
 * 首帧接收/引用计数契约由固定 capture/render 库摘要约束，本入口不伪造释放回执。
 */
#include "native_memory_runner.hpp"
#include "native_memory_request.hpp"
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
#include <functional>
#include <sstream>
#include <array>
#include <tuple>
extern char **environ;
extern "C" int cn_signal_pending(void);
extern "C" int cn_render_main(int,char**);
extern "C" int cn_counter_main(int,char**);
extern "C" int cn_publish_main(int,char**);
extern "C" int cn_prepare_main(int,char**);
extern "C" int cn_heif_restart_main(int,char**);
namespace cn { namespace memory_runner {
static const std::string module="/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so";
static const char*module_hash="9df3e1da3832cc2a6626c281f605d68314202f19a622e3126fb53b408909be94";
static const char*storage_hash="8c086e9c2a340c2a388e92ae3de39d6d0e5ce19ac3d582626112a24fd3b97aef";
static const char*quarter_hash="25bd7a3b90768c651987d11c297f31081e45b3e75c1e7a0ec76a070e99720216";
static const std::string global_lock="/tmp/x2d-multishot-active";
struct Identity {pid_t pid=0;std::string start;char state='?';};
static Identity identify(pid_t pid){
    require(pid>0,"memory-process-id",70);auto text=read("/proc/"+std::to_string(pid)+"/stat",16384);auto end=text.rfind(')');require(end!=std::string::npos,"memory-process-stat",70);
    std::istringstream in(text.substr(end+1));std::string field;Identity id;id.pid=pid;
    for(unsigned i=3;i<=22;i++){require(bool(in>>field),"memory-process-start",70);if(i==3){require(field.size()==1,"memory-process-state",70);id.state=field[0];}}
    require(digits(field)&&field.size()<=16,"memory-process-start-value",70);id.start=field;return id;
}
static bool alive(pid_t pid){if(!kill(pid,0))return true;require(errno==ESRCH,"memory-process-presence",70);return false;}
static pid_t parse_pid(const std::string&s){require(digits(s)&&s.size()<=10,"memory-recorded-pid",70);auto p=std::stoull(s);require(p>0&&p<=INT_MAX,"memory-recorded-pid-range",70);return pid_t(p);}
static std::vector<std::string> records(const std::string&text){
    std::vector<std::string> out;for(size_t at=0;at<text.size();){auto end=text.find('\0',at);require(end!=std::string::npos,"memory-process-record");out.push_back(text.substr(at,end-at));at=end+1;}return out;
}
static bool contains(const std::vector<std::string>&items,const std::string&s){for(const auto&i:items)if(i==s)return true;return false;}
static void fixed_hash(const std::string&p,const char*h){require(regular(p,true)&&sha256(p)==h,"memory-component-hash");}
static bool exact(const std::string&p,const std::string&text){return regular(p)&&read(p,text.size()+1)==text+"\n";}
static void receipt(const std::string&s,const std::string&name,const std::string&text){require(name.find('/')==std::string::npos,"memory-receipt-name",2);write_replace(s+"/"+name,text+"\n");}
static void remove_if(const std::string&p){if(exists(p))remove_regular(p);}
static void local_cancel(){if(stopping())throw Error("cancelled",71);}
static void delay(unsigned ms){try{sleep_ms(ms);}catch(const Error&e){if(e.rc==143)throw Error("cancelled",71);throw;}}
static void readonly(bool change){
    unsigned count=0;unsigned long flags=MS_REMOUNT|MS_RDONLY;bool ro=false;
    std::istringstream rows(read("/proc/self/mounts",1024*1024));std::string line;
    while(std::getline(rows,line)){std::istringstream row(line);std::string src,path,type,opts;require(bool(row>>src>>path>>type>>opts),"memory-mount-record",90);if(path!="/system")continue;count++;
        std::istringstream items(opts);std::string item;while(std::getline(items,item,',')){
            if(item=="ro")ro=true;else if(item=="nosuid")flags|=MS_NOSUID;else if(item=="nodev")flags|=MS_NODEV;else if(item=="noexec")flags|=MS_NOEXEC;else if(item=="sync")flags|=MS_SYNCHRONOUS;else if(item=="dirsync")flags|=MS_DIRSYNC;else if(item=="noatime")flags|=MS_NOATIME;else if(item=="nodiratime")flags|=MS_NODIRATIME;else if(item=="relatime")flags|=MS_RELATIME;else if(item=="strictatime")flags|=MS_STRICTATIME;
        }
    }
    require(count==1,"memory-system-mount-ambiguous",90);
    if(change){if(mount(nullptr,"/system",nullptr,flags,nullptr))throw Error("memory-system-readonly",90,true,errno);readonly(false);}else require(ro,"memory-system-not-readonly",90);
}
static void live(bool on){auto p=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.set_live_view",on?"boolean:true":"boolean:false"});require(!p.rc,"memory-live",p.rc);}
static void put(const std::string&name,const std::string&expected){
    require(name=="drive_mode"||name=="multishot_control_mode"||name=="exposures_in_multishot_session"||name=="image_format","memory-setting-name",2);
    auto current=get("camera",name,"int32");require(digits(current)&&digits(expected),"memory-setting-value");
    if(current!=expected){auto p=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=3000","--dest=com.hasselblad.camera","/camera","org.freedesktop.DBus.Properties.Set","string:com.hasselblad.camera","string:"+name,"variant:int32:"+expected});require(!p.rc,"memory-setting-write",p.rc);}
    require(get("camera",name,"int32")==expected,"memory-setting-verify");
}
static void poll_hook(){get("storage","capture_storage_mode","int32");}
static void pump(){try{poll_hook();}catch(const Error&e){if(e.rc==71||e.rc==143)throw;}catch(const std::exception&){} }
static bool idle(){return get("camera","exposure_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("storage","storage_status","int32")=="0";}
static bool mapped_identity(const std::string&maps,const std::string&path,const struct stat&st){
    std::istringstream rows(maps);std::string row;bool found=false;
    while(std::getline(rows,row)){
        std::istringstream fields(row);std::string address,perms,offset,dev,inode,name;
        require(bool(fields>>address>>perms>>offset>>dev>>inode),"memory-maps-record",90);
        std::getline(fields,name);auto at=name.find_first_not_of(' ');if(at==std::string::npos||name.substr(at)!=path)continue;
        auto colon=dev.find(':');require(colon!=std::string::npos&&colon>0&&colon+1<dev.size()&&digits(inode)&&inode.size()<=20,"memory-maps-fields",90);
        auto maj=dev.substr(0,colon),min=dev.substr(colon+1);
        require(maj.size()<=8&&min.size()<=8&&maj.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos&&min.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"memory-maps-device",90);
        require(std::stoull(inode)==uint64_t(st.st_ino)&&std::stoull(maj,nullptr,16)==uint64_t(major(st.st_dev))&&std::stoull(min,nullptr,16)==uint64_t(minor(st.st_dev)),"memory-maps-inode",90);found=true;
    }
    return found;
}
static Identity original_storage(){
    auto ps=pids("camera-storage");require(ps.size()==1,"memory-original-storage-count",22);auto id=identify(ps[0]);
    require(executable(id.pid)=="/system/bin/camera-storage","memory-original-storage-exe",22);
    auto prefix="/proc/"+std::to_string(id.pid);auto env=records(read(prefix+"/environ",131072));std::vector<std::string> loader;for(const auto&line:env)if(line.rfind("LD_",0)==0)loader.push_back(line);
    auto maps=read(prefix+"/maps",2097152);require(!maps.empty()&&maps.find(module)==std::string::npos,"memory-original-storage-maps",22);
    if(!loader.empty()){
        require(loader.size()==1&&loader[0]=="LD_PRELOAD=/system/lib64/libx2d_quarter_native.so","memory-original-storage-loader",22);
        fixed_hash("/system/lib64/libx2d_quarter_native.so",quarter_hash);
        struct stat native;require(!lstat("/system/lib64/libx2d_quarter_native.so",&native)&&S_ISREG(native.st_mode)&&native.st_nlink==1,"memory-quarter-module-identity",22);
        require(value("/tmp/x2d-quarter-native.status")=="QUARTER_NATIVE_READY"&&value("/tmp/x2d-pixel-overclock/quarter.txt")=="DISARMED"&&mapped_identity(maps,"/system/lib64/libx2d_quarter_native.so",native),"memory-quarter-state",22);
    }
    fixed_hash("/system/bin/camera-storage",storage_hash);require(pids("camera-storage")==ps&&identify(id.pid).start==id.start&&executable(id.pid)=="/system/bin/camera-storage","memory-original-storage-changed",22);return id;
}
static void stable_original(){
    auto begin=monotonic_ms();unsigned stable=0;std::string key;
    for(unsigned i=0;i<25;i++){try{auto id=original_storage();auto now=std::to_string(id.pid)+":"+id.start;if(key==now)stable++;else{key=now;stable=1;}}catch(const Error&e){if(e.rc==143||e.rc==71)throw;stable=0;key.clear();}
        require(monotonic_ms()-begin<5000,"memory-original-storage-stability",90);if(stable>=3)return;delay(100);
    }throw Error("memory-original-storage-stability",90);
}
static void owned_storage(const std::string&s,pid_t p){
    auto start=value(s+"/storage.start");require(digits(start)&&identify(p).start==start&&executable(p)=="/system/bin/camera-storage","memory-owned-storage-identity",90);
    fixed_hash(module,module_hash);struct stat mapped;require(!lstat(module.c_str(),&mapped)&&S_ISREG(mapped.st_mode)&&mapped.st_nlink==1,"memory-module-identity",90);
    auto prefix="/proc/"+std::to_string(p);require(mapped_identity(read(prefix+"/maps",2097152),module,mapped),"memory-owned-storage-module",90);
    require(contains(records(read(prefix+"/environ",131072)),"X2D_MEMORY_CAPTURE_JOB="+s),"memory-owned-storage-job",90);
    require(identify(p).start==start&&executable(p)=="/system/bin/camera-storage","memory-owned-storage-changed",90);
}
static void wait_storage_exit(pid_t p,const std::string&expected){
    // 恢复可能在 Runner 本身或其 sibling helper 内执行；只有维护 ProcessChild 的 Runner 可以 waitpid。
    // 此处只观察退出/僵尸并核对 incarnation，不消费其他所有者的子进程状态。
    for(unsigned i=0;alive(p);i++){auto id=identify(p);require(id.start==expected,"memory-storage-pid-reused",90);if(id.state=='Z')break;require(i<49,"memory-owned-storage-not-exited",90);delay(100);}
}
struct Settings {std::string drive,mode,frames,format;};
static void validate(const Settings&v){require(v.drive.size()==1&&v.drive[0]>='0'&&v.drive[0]<='6'&&v.mode.size()==1&&v.mode[0]>='0'&&v.mode[0]<='3'&&(v.frames=="1"||v.frames=="4"||v.frames=="6"||v.frames=="16")&&v.format.size()==1&&v.format[0]>='0'&&v.format[0]<='4',"memory-snapshot-rejected",20);}
static Settings settings(){Settings v{get("camera","drive_mode","int32"),get("camera","multishot_control_mode","int32"),get("camera","exposures_in_multishot_session","int32"),get("camera","image_format","int32")};validate(v);return v;}
static Settings snapshot(const std::string&s){Settings v{value(s+"/drive.before"),value(s+"/mode.before"),value(s+"/frames.before"),value(s+"/format.before")};validate(v);return v;}
static void save_settings(const std::string&s,const Settings&v,bool initial){
    std::array<std::pair<const char*,std::string>,4> items={{{"drive",v.drive},{"mode",v.mode},{"frames",v.frames},{"format",v.format}}};
    for(const auto&i:items){auto path=s+"/"+i.first+".before";if(initial)write_new(path,i.second+"\n");else{require(regular(path),"memory-snapshot-type");write_replace(path,i.second+"\n");}}
}
static std::string recovery_snapshot(const std::string&s){auto v=snapshot(s);std::string live="UNCHANGED";if(exists(s+"/live.before")){live=value(s+"/live.before");require(live=="0"||live=="1","memory-recovery-live-snapshot",90);}return "MEMORY_RESTORE_SNAPSHOT_V1\n"+v.drive+" "+v.mode+" "+v.frames+" "+v.format+" "+live+"\n";}
static void task_drained(const std::string&s){
    if(exists(s+"/capture.invoked"))require(regular(s+"/capture.invoked")&&exact(s+"/memory.consumers.drained","MEMORY_CONSUMERS_DRAINED_V1"),"memory-consumers-unconfirmed",90);
    if(exists(s+"/render.started"))require(regular(s+"/render.started")&&exact(s+"/render.drained","RENDER_DRAINED_V1"),"memory-render-drain-unconfirmed",90);
}
static void all_drained(const std::string&s){task_drained(s);if(exists(s+"/render.started"))require(get("camera","stream_group","int32")=="255","memory-stream-drain-unconfirmed",90);}
static void note(const std::string&s,const char*why)noexcept{try{if(!exists(s+"/restore.deferred"))receipt(s,"restore.deferred",why);}catch(const std::exception&e){fprintf(stderr,"MEMORY_RESTORE_NOTE_UNAVAILABLE reason=%s detail=%s\n",why,e.what());}puts(why);}
static void restore_check(const std::string&s,bool diagnostics){
    if(diagnostics)receipt(s,"restore.check.step","MARKERS");
    require(exact(s+"/restored","RESTORED_V1")&&exact(s+"/restore.phase","COMPLETE")&&exact(s+"/restore.rc","0"),"memory-restore-markers",90);task_drained(s);
    require(read(s+"/restore.snapshot",128)==recovery_snapshot(s),"memory-restored-snapshot-receipt",90);
    if(diagnostics)receipt(s,"restore.check.step","SYSTEM_READ_ONLY");readonly(false);
    if(diagnostics)receipt(s,"restore.check.step","ORIGINAL_STORAGE");stable_original();
    if(diagnostics)receipt(s,"restore.check.step","COMPLETE");
}
struct DirOwner {
    std::string path;struct stat id{};bool held=false;
    explicit DirOwner(std::string p):path(std::move(p)){}
    void acquire(){mkdir_new(path);held=true;require(!lstat(path.c_str(),&id)&&S_ISDIR(id.st_mode),"memory-lock-identity",90);}
    void release(){struct stat now;require(held&&id.st_ino&&!lstat(path.c_str(),&now)&&S_ISDIR(now.st_mode)&&now.st_dev==id.st_dev&&now.st_ino==id.st_ino,"memory-lock-replaced",90);if(rmdir(path.c_str()))throw Error("memory-unlock",90,true,errno);held=false;}
};
static int restore(const std::string&s,bool diagnostics=false){
    Recovery recovery;int primary=0;bool marker_owned=false;struct stat marker{};DirOwner lock(s+"/restore.lock");
    if(exists(s+"/restored")){restore_check(s,diagnostics);return 0;}
    for(unsigned i=0;;i++){
        if(!mkdir(lock.path.c_str(),0700)){lock.held=true;require(!lstat(lock.path.c_str(),&lock.id)&&S_ISDIR(lock.id.st_mode),"memory-restore-lock-identity",90);break;}
        require(errno==EEXIST,"memory-restore-lock-create",90);require(directory(lock.path),"memory-restore-lock-type",90);
        if(exists(s+"/restored")){restore_check(s,diagnostics);return 0;}require(i<29,"memory-restore-lock-busy",90);delay(1000);
    }
    auto phase=[&](const char*p){receipt(s,"restore.phase",p);};
    try{
        if(exists(s+"/restored")){restore_check(s,diagnostics);lock.release();return 0;}
        receipt(s,"restore.rc","125");phase("DRAIN_GUARD");bool drained=false;
        for(unsigned i=0;i<30;i++){try{all_drained(s);drained=true;break;}catch(const Error&){}pump();delay(200);}
        if(!drained){note(s,"RESTORE_DEFERRED_MEMORY_CONSUMERS_UNCONFIRMED");throw Error("memory-consumers-unconfirmed",90);}
        phase("EXPOSURE_IDLE");auto state=get("camera","exposure_status","int32");
        if(state!="0"){
            require(regular(s+"/capture.invoked"),"memory-not-owned-exposure",90);
            try{command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=180000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.stop_session"},180000,false);}catch(const std::exception&){}
            for(unsigned i=0;state!="0"&&i<30;i++){delay(1000);state=get("camera","exposure_status","int32");pump();}
        }
        require(state=="0","memory-owned-exposure-not-idle",90);phase("STORAGE_IDLE");
        for(unsigned i=0;get("storage","storage_status","int32")!="0";i++){require(i<30,"memory-storage-io-active",90);pump();delay(1000);}
        bool setting_failure=false;
        if(exists(s+"/settings.modified")){
            require(regular(s+"/settings.modified"),"memory-settings-marker-type",90);phase("SNAPSHOT");auto v=snapshot(s);
            std::array<std::tuple<const char*,const char*,std::string>,4> values={{{"DRIVE_MODE","drive_mode",v.drive},{"MULTISHOT_MODE","multishot_control_mode",v.mode},{"FRAME_COUNT","exposures_in_multishot_session",v.frames},{"IMAGE_FORMAT","image_format",v.format}}};
            for(const auto&i:values){phase(std::get<0>(i));try{put(std::get<1>(i),std::get<2>(i));}catch(const Error&e){setting_failure=true;fprintf(stderr,"MEMORY_RESTORE_SETTING step=%s rc=%d errno=%d\n",std::get<0>(i),e.rc,e.system_errno);note(s,"RESTORE_SETTINGS_NOT_CONFIRMED");}catch(const std::exception&){setting_failure=true;fprintf(stderr,"MEMORY_RESTORE_SETTING step=%s rc=1 errno=0\n",std::get<0>(i));note(s,"RESTORE_SETTINGS_NOT_CONFIRMED");}}
        }
        phase("OWNED_STORAGE");
        if(exists(s+"/storage.pid")){
            auto p=parse_pid(value(s+"/storage.pid"));if(alive(p)){
                auto expected=value(s+"/storage.start");auto current=identify(p);require(current.start==expected,"memory-storage-pid-reused",90);
                if(current.state!='Z'){owned_storage(s,p);require(!kill(p,SIGTERM)||errno==ESRCH,"memory-owned-storage-term",90);}
                wait_storage_exit(p,expected);
            }
        }
        fixed_hash(module,module_hash);phase("SYSTEM_READ_ONLY");readonly(true);
        if(exists(s+"/storage.stopped")){
            require(regular(s+"/storage.stopped"),"memory-storage-stopped-type",90);phase("ORIGINAL_STORAGE_START");service("camera-storage",true);
            bool valid=false;for(unsigned i=0;i<20;i++){try{original_storage();valid=true;break;}catch(const Error&){}delay(1000);}require(valid,"memory-original-storage-unconfirmed",90);
        }
        phase("ORIGINAL_STORAGE_VERIFY");original_storage();require(!setting_failure,"memory-settings-restore-unconfirmed",90);
        if(exists(s+"/live.before")){phase("LIVE_VIEW");auto expected=value(s+"/live.before");require(expected=="0"||expected=="1","memory-live-snapshot",90);if(get("camera","live_view_state","int32")!=expected)live(expected=="1");require(get("camera","live_view_state","int32")==expected,"memory-live-restore",90);}
        phase("FINAL_READ_ONLY");readonly(true);sync();remove_if(s+"/restore.deferred");write_replace(s+"/restore.snapshot",recovery_snapshot(s));phase("COMPLETE");receipt(s,"restore.rc","0");
        write_new(s+"/restored","RESTORED_V1\n");marker_owned=true;require(!lstat((s+"/restored").c_str(),&marker),"memory-restored-identity",90);sync();
        puts("PRIVATE_CAPTURE_ORIGINAL_SETTINGS_STORAGE_LIVE_AND_READ_ONLY_RESTORED");
    }catch(const Error&e){primary=e.rc?e.rc:90;fprintf(stderr,"MEMORY_RESTORE_PRIMARY step=%s rc=%d errno=%d\n",e.what(),primary,e.system_errno);}
    catch(const std::exception&e){primary=90;fprintf(stderr,"MEMORY_RESTORE_PRIMARY detail=%s rc=90\n",e.what());}
    try{readonly(true);}catch(const std::exception&){primary=90;}
    try{lock.release();}catch(const std::exception&){primary=90;}
    if(primary&&marker_owned){try{struct stat now;require(marker.st_ino&&!lstat((s+"/restored").c_str(),&now)&&now.st_dev==marker.st_dev&&now.st_ino==marker.st_ino,"memory-restored-replaced",90);remove_regular(s+"/restored");}catch(const std::exception&){primary=90;}}
    try{receipt(s,"restore.rc",std::to_string(primary));if(primary)note(s,"RESTORE_STEP_FAILED");}catch(const std::exception&){primary=90;}
    if(!primary&&diagnostics)restore_check(s,true);return primary;
}
static int call(int(*entry)(int,char**),std::vector<std::string> args){std::vector<char*> raw;for(auto&a:args)raw.push_back(a.data());return entry(int(raw.size()),raw.data());}
static void native_result(int rc,const char*step){if(rc)throw Error(step,rc,rc==90);}
/* 本文件专用子进程。所有可执行目标/回调由下方固定枚举选定，绝不接受外部程序名。
 * 析构不kill、不阻塞；引用释放未确认由主恢复保锁，watchdog继续同一恢复协议。
 */
struct ProcessChild {
    pid_t pid=-1;std::string start;bool done=false;int rc=125;
    bool poll(){if(done)return true;require(pid>0,"memory-child-not-started");int status;pid_t got;do{got=waitpid(pid,&status,WNOHANG);}while(got<0&&errno==EINTR);require(got>=0,"memory-child-poll");if(got==pid){done=true;rc=WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?128+WTERMSIG(status):1;}return done;}
    int wait(){while(!poll())delay(20);return rc;}
    void launch(const std::string&log,const std::function<int()>&body,const std::function<void()>&registered={},bool discard_output=false){
        local_cancel();require(pid<0,"memory-child-reused",2);int output=discard_output?open("/dev/null",O_WRONLY|O_CLOEXEC):open(log.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);require(output>=0,"memory-child-log");
        int original_pipe[2];if(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,original_pipe)){int cause=errno;close(output);throw Error("memory-child-gate",1,false,cause);}
        int gate_read=fcntl(original_pipe[0],F_DUPFD_CLOEXEC,3),gate_write=fcntl(original_pipe[1],F_DUPFD_CLOEXEC,3);int gate_errno=errno;close(original_pipe[0]);close(original_pipe[1]);
        if(gate_read<0||gate_write<0){if(gate_read>=0)close(gate_read);if(gate_write>=0)close(gate_write);close(output);throw Error("memory-child-gate-fd",1,false,gate_errno);}
        fflush(nullptr);pid=fork();int cause=errno;
        if(pid<0){close(output);close(gate_read);close(gate_write);throw Error("memory-child-fork",1,false,cause);}
        if(!pid){
            close(gate_write);char permission=0;ssize_t got;do{got=::read(gate_read,&permission,1);}while(got<0&&errno==EINTR);close(gate_read);if(got!=1||permission!='G')_exit(126);
            int copy=fcntl(output,F_DUPFD_CLOEXEC,3);if(copy<0||dup2(copy,1)<0||dup2(copy,2)<0)_exit(126);close(copy);if(output!=1&&output!=2)close(output);
            int input=open("/dev/null",O_RDONLY|O_CLOEXEC);if(input<0||dup2(input,0)<0)_exit(126);if(input!=0)close(input);
            int result=1;try{result=body();}catch(const Error&e){int primary=e.rc==143?71:e.rc;result=e.recovery_failed?90:primary;fprintf(stderr,"MEMORY_CHILD_FAILURE step=%s primary_rc=%d restore_rc=%d errno=%d\n",e.what(),primary,e.recovery_failed?90:0,e.system_errno);}catch(const std::exception&e){fprintf(stderr,"MEMORY_CHILD_FAILURE detail=%s\n",e.what());}
            fflush(nullptr);_exit(result);
        }
        close(output);close(gate_read);
        try{start=identify(pid).start;local_cancel();if(registered)registered();local_cancel();char permission='G';ssize_t sent;do{sent=send(gate_write,&permission,1,MSG_NOSIGNAL);}while(sent<0&&errno==EINTR);require(sent==1,"memory-child-gate-release");close(gate_write);}
        catch(...){close(gate_write);throw;}
    }
};
enum class Tool {Inputs,Fix,Pack,Storage};
static bool inherit_environment(const std::string&line){return line.rfind("LD_",0)!=0&&line.rfind("X2D_",0)!=0;}
static int exec_tool(Tool tool,const std::vector<std::string>&extra,const std::string&jobpath={}){
    const char*path=tool==Tool::Inputs?"/system/bin/x2d-pixel-overclock/inputs":tool==Tool::Fix?"/system/bin/x2d-pixel-overclock/fix-3fr":tool==Tool::Pack?"/system/bin/x2d-pixel-overclock/pack":"/system/bin/camera-storage";
    const char*digest=tool==Tool::Inputs?runtime::pixel_inputs_sha:tool==Tool::Fix?runtime::pixel_fix_3fr_sha:tool==Tool::Pack?"9521d4cf0a2ea01b039d71d991d507042c518c86c477ca7d5295912c19314b28":storage_hash;fixed_hash(path,digest);
    std::vector<std::string> args={path};args.insert(args.end(),extra.begin(),extra.end());std::vector<char*> av;for(auto&a:args)av.push_back(a.data());av.push_back(nullptr);
    // H3 frozen sources 的 getenv 引用仅需要本路由三项；丢弃 disk/raw-stack/ABI 调试等过期 X2D 开关。
    std::vector<std::string> env;for(char**p=environ;p&&*p;p++){std::string item=*p;if(inherit_environment(item))env.push_back(item);}
    if(tool==Tool::Storage){require(!jobpath.empty()&&extra.empty(),"memory-storage-launch-contract",2);env.push_back("X2D_PRIVATE_CAPTURE_ABI=1");env.push_back("X2D_MEMORY_CAPTURE_JOB="+jobpath);env.push_back("LD_PRELOAD="+module);}
    std::vector<char*> ev;for(auto&e:env)ev.push_back(e.data());ev.push_back(nullptr);signal(SIGHUP,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);execve(path,av.data(),ev.data());return 127;
}
static void tool(ProcessChild&child,const std::string&s,Tool which,const std::vector<std::string>&args,const std::string&log){require(child.pid<0||child.done,"memory-utility-active",90);child=ProcessChild{};child.launch(s+"/"+log,[&](){return exec_tool(which,args);});int rc=child.wait();require(!rc,"memory-tool-failed",rc);}
static void run_identity(const std::string&s,pid_t owner){
    require(identify(owner).start==value(s+"/run.start"),"memory-run-start-changed",70);struct stat own,other;require(!stat("/proc/self/exe",&own)&&!stat(("/proc/"+std::to_string(owner)+"/exe").c_str(),&other)&&own.st_dev==other.st_dev&&own.st_ino==other.st_ino,"memory-run-executable-changed",70);
    auto argv=records(read("/proc/"+std::to_string(owner)+"/cmdline",16384));require(argv.size()>=3&&argv[1]=="memory-run"&&argv[2]==s,"memory-run-command-changed",70);
}
static int watchdog(const std::string&s,pid_t owner){
    Recovery recovery;require(digits(value(s+"/run.start")),"memory-watchdog-start-record",70);
    // 未回收的僵尸仍会响应 kill(pid,0)，但已没有可读取的 exe；应进入恢复，不能在身份读取中提前退出。
    auto owner_current=[&](){if(!alive(owner))return false;try{auto id=identify(owner);if(id.state=='Z'||id.start!=value(s+"/run.start"))return false;run_identity(s,owner);return true;}catch(const Error&){if(!alive(owner))return false;auto id=identify(owner);if(id.state=='Z'||id.start!=value(s+"/run.start"))return false;throw;}};
    while(!exists(s+"/manual.trigger")&&!exists(s+"/run.rc")&&owner_current())delay(1000);
    for(unsigned elapsed=0;exists(s+"/manual.trigger")&&!exists(s+"/run.rc")&&owner_current()&&elapsed<1200;elapsed++)delay(1000);
    if(!exists(s+"/run.rc")){if(owner_current()){run_identity(s,owner);require(!kill(owner,SIGTERM)||errno==ESRCH,"memory-watchdog-term",70);delay(2000);}return restore(s);}
    return 0;
}
static bool valid_phase(const std::string&p){for(const auto*x:{"prepare","delay","capture","merge","render","raw","convert","publish","restore"})if(p==x)return true;return false;}
struct Runner {
    const std::string s;std::string phase="preflight",step="NONE",live_before,shutter;DirOwner lock;ProcessChild render,restorer,storage,capture,watch,utility;
    explicit Runner(std::string path):s(std::move(path)),lock(global_lock){}
    void phase_end(){if(!exists(s+"/phase-clock"))return;std::istringstream in(value(s+"/phase-clock"));std::string previous,stamp,extra;require(bool(in>>previous>>stamp)&&!(in>>extra)&&valid_phase(previous)&&digits(stamp)&&stamp.size()<=12,"memory-phase-clock");auto now=monotonic_ms();auto before=std::stoull(stamp);require(now>=before,"memory-phase-clock-order");append(s+"/phase-metrics","PHASE_"+previous+"_MS "+std::to_string(now-before)+"\n");}
    void stage(const std::string&p){require(valid_phase(p),"memory-stage",2);phase=p;step="NONE";try{phase_end();receipt(s,"phase-clock",p+" "+std::to_string(monotonic_ms()));}catch(const std::exception&){fputs("PIXEL_TIMING_UNAVAILABLE\n",stderr);}receipt(s,"stage",p);}
    void timed(const std::string&name,const std::function<void()>&fn){step=name;auto before=monotonic_ms();fn();append(s+"/finish-metrics","FINISH_"+name+"_MS "+std::to_string(monotonic_ms()-before)+"\n");}
    void first_error(int rc){if(!exists(s+"/first_error"))write_new(s+"/first_error","phase="+phase+"\nstep="+step+"\nrc="+std::to_string(rc)+"\n");}
    void shutter_ok(){auto now=get("camera","eshutter_current","boolean");require((now=="true"||now=="false")&&(shutter.empty()||now==shutter),"memory-shutter-state",19);}
    void capture_requirements(){shutter_ok();require(get("storage","capture_storage_mode","int32")=="1","memory-local-storage-required",19);auto depth=get("camera","image_bit_depth","int32");require(depth=="0"||depth=="1","memory-bit-depth",19);}
    void observe_errors(){require(!exists(s+"/failed")&&!exists(s+"/memory.failed"),"memory-capture-handoff-failed",24);}
    int cleanup(int primary,bool prior_recovery_failure){
        Recovery recovery;int problem=prior_recovery_failure?90:0,restored_rc=125;std::string cleanup_step=prior_recovery_failure?"PRIOR_RECOVERY":"JOIN_RESTORE",reason=prior_recovery_failure?"PRIOR_CLEANUP_UNCONFIRMED":"NONE";bool unlocked=false;
        if(!primary&&cn_signal_pending())primary=71;
        auto fail=[&](const char*where,const char*why){if(!problem){problem=90;cleanup_step=where;reason=why;}};
        auto safe=[&](const char*where,const char*why,const std::function<void()>&fn){try{fn();}catch(const std::exception&e){fail(where,why);fprintf(stderr,"MEMORY_CLEANUP_FAILURE step=%s detail=%s\n",where,e.what());}};
        if(primary)safe("WRITE_RECEIPTS","RECEIPT_WRITE_FAILED",[&](){first_error(primary);});
        safe("STAGE","STAGE_WRITE_FAILED",[&](){stage("restore");remove_if(s+"/prepared.ready");});
        safe("WRITE_RECEIPTS","RECEIPT_WRITE_FAILED",[&](){receipt(s,"cleanup.restore.rc","125");receipt(s,"cleanup.step","JOIN_RESTORE");receipt(s,"cleanup.reason","NONE");});
        if(utility.pid>0&&!utility.done)safe("WAIT_UTILITY","UTILITY_PROCESS_ACTIVE",[&](){for(unsigned i=0;!utility.poll();i++){require(i<49,"memory-utility-still-active",90);delay(200);}});
        if(restorer.pid>0)safe("JOIN_RESTORE","RESTORE_JOIN_FAILED",[&](){require(!restorer.wait(),"memory-restore-child",90);});
        if(render.pid>0&&!render.done)safe("WAIT_RENDER","RENDER_PROCESS_ACTIVE",[&](){for(unsigned i=0;!render.poll();i++){require(i<49,"memory-render-still-active",90);pump();delay(200);}});
        safe("VERIFY_RESTORED","FINAL_RESTORE_CHECK_FAILED",[&](){receipt(s,"restore.check.rc","125");receipt(s,"restore.check.step","MARKERS");restored_rc=restore(s,true);receipt(s,"restore.check.rc",std::to_string(restored_rc));require(!restored_rc,"memory-final-restore",90);});
        if(capture.pid>0&&!capture.done)safe("WAIT_CAPTURE_RPC","CAPTURE_RPC_PROCESS_ACTIVE",[&](){for(unsigned i=0;!capture.poll();i++){require(i<49,"memory-capture-rpc-still-active",90);delay(200);}});
        if(storage.pid>0&&!storage.done)safe("WAIT_STORAGE","STORAGE_PROCESS_ACTIVE",[&](){require(storage.poll(),"memory-storage-still-active",90);});
        if(exists(s+"/render.started"))safe("VERIFY_IPC_CLEANED","IPC_CLEANUP_UNCONFIRMED",[&](){require(exact(s+"/render.cleaned","RENDER_CLEANED_V1"),"memory-ipc-cleanup",90);});
        try{phase_end();remove_if(s+"/phase-clock");}catch(const std::exception&){fputs("PIXEL_TIMING_UNAVAILABLE\n",stderr);}
        if(!problem)cleanup_step="RELEASE_GLOBAL_LOCK";
        safe("WRITE_RECEIPTS","RECEIPT_WRITE_FAILED",[&](){receipt(s,"cleanup.restore.rc",std::to_string(restored_rc));receipt(s,"cleanup.reason",reason);receipt(s,"cleanup.step",cleanup_step);receipt(s,"cleanup.rc",std::to_string(problem));require(exact(s+"/cleanup.restore.rc",std::to_string(restored_rc))&&exact(s+"/cleanup.reason",reason)&&exact(s+"/cleanup.step",cleanup_step)&&exact(s+"/cleanup.rc",std::to_string(problem)),"memory-cleanup-receipt-verify",90);sync();});
        if(!problem&&!restored_rc)safe("RELEASE_GLOBAL_LOCK","GLOBAL_LOCK_RELEASE_FAILED",[&](){lock.release();unlocked=true;});
        if(!problem){cleanup_step="COMPLETE";safe("WRITE_RECEIPTS","RECEIPT_WRITE_FAILED",[&](){receipt(s,"cleanup.step","COMPLETE");});}
        if(problem){primary=90;try{receipt(s,"cleanup.step",cleanup_step);receipt(s,"cleanup.reason",reason);receipt(s,"cleanup.rc",std::to_string(problem));}catch(const std::exception&){} }
        if(!primary&&cn_signal_pending())primary=71;
        if(primary)try{first_error(primary);}catch(const std::exception&){}
        try{write_new(s+"/run.rc",std::to_string(primary)+"\n");}catch(const std::exception&){primary=90;fail("WRITE_RECEIPTS","RECEIPT_WRITE_FAILED");try{receipt(s,"cleanup.step",cleanup_step);receipt(s,"cleanup.reason",reason);receipt(s,"cleanup.rc","90");}catch(const std::exception&){} }
        if(unlocked&&problem)puts("CLEANUP_POST_UNLOCK_RECEIPT_FAILED");sync();return primary;
    }
    int run(unsigned seconds){
        auto dispatcher=identify(getppid());lock.acquire();bool armed_cleanup=false,prior_recovery_failure=false;int primary=0;
        try{
            step="LOCK";mkdir_new(s+"/run.lock");step="DESTINATIONS";struct statvfs space;require(!statvfs(s.c_str(),&space),"memory-archive-space-unknown",28);uint64_t block=space.f_frsize?space.f_frsize:space.f_bsize;require(block>0,"memory-archive-block-size",28);constexpr uint64_t needed=uint64_t(6291456)*1024;require(uint64_t(space.f_bavail)>=needed/block+(needed%block!=0),"memory-archive-requires-6gib",28);
            require(!exists(s+"/capture.log"),"memory-capture-log-exists");for(unsigned i=0;i<6;i++)require(!exists(s+"/frame-"+std::to_string(i)+".3FR"),"memory-source-exists");
            step="MODE_CACHE";bool prewarmed=!call(cn_prepare_main,{"native","mode-prepare","--check"});
            fixed_hash("/system/bin/x2d-pixel-overclock/fix-3fr",runtime::pixel_fix_3fr_sha);
            if(!prewarmed){fixed_hash(module,module_hash);fixed_hash("/system/bin/camera-storage",storage_hash);fixed_hash("/system/lib64/libQt6Core.so.6","ab64228f088d7404835e59f97185e14cea904d301dfefd29b78a9a5d76e657c4");unsigned stable=0;for(unsigned i=0;stable<5;i++){require(i<200,"memory-native-idle-timeout");stable=idle()?stable+1:0;if(stable<5)delay(100);}}
            capture_requirements();stage("prepare");step="USER_SETTINGS";save_settings(s,settings(),true);step="ORIGINAL_PROCESS";original_storage();readonly(true);armed_cleanup=true;
            auto self=identify(getpid());write_new(s+"/run.start",self.start+"\n");watch.launch(s+"/watchdog.log",[&](){return watchdog(s,self.pid);});
            readonly(true);require(!exists(s+"/manual.trigger"),"memory-trigger-reused");stage("delay");write_new(s+"/prepared.ready","PREPARED\n");puts("MEMORY_MODE_ARMED_ORIGINAL_STORAGE_WAITING_MANUAL_SHUTTER");
            while(!exists(s+"/manual.trigger")){local_cancel();require(alive(dispatcher.pid)&&identify(dispatcher.pid).start==dispatcher.start,"memory-dispatcher-lost",71);job(s);observe_errors();delay(50);}
            require(exact(s+"/manual.trigger","GO"),"memory-trigger-invalid",19);remove_regular(s+"/prepared.ready");puts("MEMORY_PREPARED_MANUAL_SHUTTER_ACCEPTED");stage("delay");
            for(unsigned left=seconds;left;left--){cancel_before_capture(s);receipt(s,"countdown",std::to_string(left));delay(1000);}cancel_before_capture(s);
            step="TRIGGER_ADMISSION";original_storage();fixed_hash(module,module_hash);require(idle(),"memory-trigger-not-idle",19);capture_requirements();auto v=settings();save_settings(s,v,false);require(regular(s+"/format-source"),"memory-format-source");receipt(s,"format-source",v.format);
            live_before=get("camera","live_view_state","int32");require(live_before=="0"||live_before=="1","memory-live-before",19);write_new(s+"/live.before",live_before+"\n");
            tool(utility,s,Tool::Inputs,{"plan-output","/mnt/media_rw/ssd/DCIM",s+"/output-plan"},"plan.log");native_result(call(cn_counter_main,{"native","counter-sync",s,"before"}),"memory-counter-before");shutter=get("camera","eshutter_current","boolean");
            step="STORAGE_TAKEOVER";write_new(s+"/storage.stopped","");sync();service("camera-storage",false);for(unsigned i=0;!pids("camera-storage").empty();i++){require(i<49,"memory-storage-stop-timeout",23);delay(100);}
            // 与 p118 相同，原厂 storage 的 stdout/stderr 不落盘，避免把服务的照片相关输出带入诊断。
            storage.launch({},[&](){return exec_tool(Tool::Storage,{},s);},[&](){write_new(s+"/storage.start",storage.start+"\n");write_new(s+"/storage.pid",std::to_string(storage.pid)+"\n");sync();},true);
            for(unsigned i=0;!exists(s+"/hook.ready");i++){require(!storage.poll(),"memory-storage-early-exit",23);pump();observe_errors();require(i<199,"memory-hook-ready-timeout",23);delay(100);}require(regular(s+"/hook.ready"),"memory-hook-ready-type");owned_storage(s,storage.pid);
            cancel_before_capture(s);stage("capture");write_new(s+"/settings.modified","");sync();put("image_format","0");put("multishot_control_mode","2");put("exposures_in_multishot_session","6");put("drive_mode","6");
            memory_request_publish(s);sync();poll_hook();
            require(regular(s+"/owner.ready")&&!exists(s+"/failed")&&get("camera","exposure_status","int32")=="0"&&get("error","active_error","boolean")=="false","memory-capture-owner",19);shutter_ok();require(get("storage","capture_storage_mode","int32")=="1","memory-capture-local",19);
            write_new(s+"/capture.invoked","");sync();
            capture.launch(s+"/capture-rpc.log",[&](){Recovery detached;int rc=1;try{rc=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=180000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.do_exposure_extended2","boolean:true","int32:24","int32:60000"},180000,false).rc;}catch(const Error&e){rc=e.rc;}write_new(s+"/capture.rc",std::to_string(rc)+"\n");sync();return rc;});puts("PRIVATE_CAPTURE_REQUEST_SUBMITTED_ONCE");
            for(unsigned i=0;;i++){require(!storage.poll(),"memory-storage-lost",24);pump();observe_errors();require(get("error","active_error","boolean")=="false","memory-capture-native-error",25);if(regular(s+"/memory.inputs.ready")&&get("camera","exposure_status","int32")=="0")break;require(i<179,"memory-group-timeout",26);delay(500);}
            // 拍摄可能重新开启取景；不能仅依赖进入任务时的快照。
            // 请求返回不代表流已停止，保留渲染器的独立状态检查。
            {auto state=get("camera","live_view_state","int32");require(state=="0"||state=="1","memory-live-state",27);
             if(state!="0")live(false);
             auto until=monotonic_ms()+10000;
             for(;;){local_cancel();require(!storage.poll(),"memory-storage-lost",27);pump();observe_errors();
              require(get("error","active_error","boolean")=="false","memory-live-stop-native-error",27);
              if(get("camera","live_view_state","int32")=="0"&&get("camera","stream_group","int32")=="255")break;
              require(monotonic_ms()<until,"memory-live-stop-timeout",27);delay(50);}}
            stage("merge");render.launch(s+"/render.log",[&](){return call(cn_render_main,{"native","render-memory",s});});auto began=monotonic_ms();bool native_started=false;
            for(unsigned i=0;!regular(s+"/memory.ready");i++){
                if(!native_started&&exists(s+"/probe.log")&&read(s+"/probe.log",16*1024*1024).find("TILE_SUBMIT 0\n")!=std::string::npos){stage("render");native_started=true;}
                require(!storage.poll(),"memory-storage-lost",27);pump();observe_errors();if(render.poll()){native_result(render.rc,"memory-render-process");require(regular(s+"/memory.ready")&&regular(s+"/render.restored"),"memory-render-exit-without-completion",27);break;}
                require(monotonic_ms()-began<900000&&i<4499,"memory-render-total-deadline",27);delay(200);
            }
            native_result(render.wait(),"memory-render-process");require(regular(s+"/render.restored"),"memory-render-result",27);
            for(unsigned i=0;!exists(s+"/capture.rc");i++){poll_hook();require(i<99,"memory-capture-rpc-receipt-timeout",27);delay(200);}require(value(s+"/capture.rc")=="0","memory-capture-rpc-result",27);
            restorer.launch(s+"/finish-restore.log",[&](){auto begin=monotonic_ms();int rc=restore(s);append(s+"/finish-metrics","FINISH_RESTORE_MS "+std::to_string(monotonic_ms()-begin)+"\n");return rc;});timed("RESTORE_JOIN",[&](){require(!restorer.wait(),"memory-restore-child",90);});step="RESTORE_RECEIPT";require(exact(s+"/restored","RESTORED_V1"),"memory-restored-receipt",90);
            stage("raw");timed("MERGED_PREVIEW",[&](){tool(utility,s,Tool::Fix,{"--job",s},"fix-3fr.log");});timed("JPEG_PACKAGE",[&](){tool(utility,s,Tool::Pack,{s+"/render.jpg",s+"/preview.jpg",s+"/album.jpg","--direct-staged-output","--capture-raw",s+"/final.3FR"},"pack.log");});
            auto format=value(s+"/format-source");require(format.size()==1&&format[0]>='0'&&format[0]<='4',"memory-publish-format");if(format=="3"||format=="4"){stage("convert");native_result(call(cn_heif_restart_main,{"native","heif-restart",s}),"memory-heif-convert");}
            stage("publish");timed("PUBLISH",[&](){native_result(call(cn_publish_main,{"native","publish-memory",s,format}),"memory-publish");});timed("COUNTER",[&](){native_result(call(cn_counter_main,{"native","counter-sync",s,"after"}),"memory-counter-after");});puts("MEMORY_MANUAL_NATIVE_RAW_JPEG_AND_ALBUM_COMPLETE");
        }catch(const Error&e){primary=e.rc==143?71:e.rc;prior_recovery_failure=e.recovery_failed;fprintf(stderr,"MEMORY_RUN_PRIMARY phase=%s step=%s error=%s rc=%d restore_unconfirmed=%d errno=%d\n",phase.c_str(),step.c_str(),e.what(),primary,prior_recovery_failure,e.system_errno);}
        catch(const std::exception&e){primary=1;fprintf(stderr,"MEMORY_RUN_PRIMARY phase=%s step=%s detail=%s rc=1\n",phase.c_str(),step.c_str(),e.what());}
        if(armed_cleanup)return cleanup(primary,prior_recovery_failure);
        Recovery recovery;try{lock.release();}catch(const std::exception&){primary=90;}return primary;
    }
};
} }
extern "C" int cn_memory_runner_main(int argc,char**argv){
    using namespace cn;using namespace cn::memory_runner;
    try{
        require(argc>=3,"memory-arguments",2);std::string mode=argv[1],path=argv[2];job(path);
        if(mode=="memory-run"){require(argc==6&&std::string(argv[4])=="6"&&std::string(argv[5])=="--prepare","memory-run-arguments",2);std::string input=argv[3];require(digits(input)&&input.size()<=2,"memory-delay",2);auto seconds=std::stoul(input);require(seconds>=2&&seconds<=60,"memory-delay-range",2);Runner runner(path);return runner.run(unsigned(seconds));}
        if(mode=="memory-watchdog"){require(argc==4,"memory-watchdog-arguments",2);return watchdog(path,parse_pid(argv[3]));}
        if(mode=="memory-restore"){
            require(argc==3,"memory-restore-arguments",2);Recovery recovery;receipt(path,"restore.check.rc","125");receipt(path,"restore.check.step","MARKERS");int rc=90;
            try{rc=restore(path,true);}catch(const Error&e){rc=e.rc;}catch(const std::exception&){}receipt(path,"restore.check.rc",std::to_string(rc));return rc;
        }throw Error("memory-command",2);
    }catch(const Error&e){int rc=e.recovery_failed?90:e.rc==143?71:e.rc;fprintf(stderr,"MEMORY_RUN_REFUSED step=%s rc=%d\n",e.what(),rc);return rc;}
    catch(const std::exception&e){fprintf(stderr,"MEMORY_RUN_REFUSED detail=%s rc=1\n",e.what());return 1;}
}
