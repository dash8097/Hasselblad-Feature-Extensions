/* RAW stack p05 SHA37de2c0d…原生迁移；仅 identity/settings/fork 辅助取自 memory 候选，堆栈协议独立实现。 */
#include "native_stack_runner.hpp"
#include "native_common.hpp"
#include "native_runtime_tables.hpp"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
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
#include "native_process.hpp"
extern "C" {
#include "camera_native.h"
}
namespace cn { namespace stack_runner {
static const std::string module="/system/bin/x2d-raw-stack/libx2d_raw_stack.so";
static const char*module_hash="332d3d27647389e730f05727607777a877fb8132d71ee52d179053bee815cd16";
static const char*storage_hash="8c086e9c2a340c2a388e92ae3de39d6d0e5ce19ac3d582626112a24fd3b97aef";
static const char*quarter_hash="25bd7a3b90768c651987d11c297f31081e45b3e75c1e7a0ec76a070e99720216";
static const std::string global_lock="/tmp/x2d-multishot-active";
struct Identity {pid_t pid=0;std::string start;char state='?';};
static Identity identify(pid_t pid){
    require(pid>0,"stack-process-id",70);auto text=read("/proc/"+std::to_string(pid)+"/stat",16384);auto end=text.rfind(')');require(end!=std::string::npos,"stack-process-stat",70);
    std::istringstream in(text.substr(end+1));std::string field;Identity id;id.pid=pid;
    for(unsigned i=3;i<=22;i++){require(bool(in>>field),"stack-process-start",70);if(i==3){require(field.size()==1,"stack-process-state",70);id.state=field[0];}}
    require(digits(field)&&field.size()<=16,"stack-process-start-value",70);id.start=field;return id;
}
static bool alive(pid_t pid){if(!kill(pid,0))return true;require(errno==ESRCH,"stack-process-presence",70);return false;}
static pid_t parse_pid(const std::string&s){require(digits(s)&&s.size()<=10,"stack-recorded-pid",70);auto p=std::stoull(s);require(p>0&&p<=INT_MAX,"stack-recorded-pid-range",70);return pid_t(p);}
static std::vector<std::string> records(const std::string&text){
    std::vector<std::string> out;for(size_t at=0;at<text.size();){auto end=text.find('\0',at);require(end!=std::string::npos,"stack-process-record");out.push_back(text.substr(at,end-at));at=end+1;}return out;
}
static bool contains(const std::vector<std::string>&items,const std::string&s){for(const auto&i:items)if(i==s)return true;return false;}
static void fixed_hash(const std::string&p,const char*h){require(regular(p,true)&&sha256(p)==h,"stack-component-hash");}
static bool exact(const std::string&p,const std::string&text){return regular(p)&&read(p,text.size()+1)==text+"\n";}
static void receipt(const std::string&s,const std::string&name,const std::string&text){require(name.find('/')==std::string::npos,"stack-receipt-name",2);write_replace(s+"/"+name,text+"\n");}
static void remove_if(const std::string&p){if(exists(p))remove_regular(p);}
static void local_cancel(){if(stopping())throw Error("cancelled",71);}
static void delay(unsigned ms){try{sleep_ms(ms);}catch(const Error&e){if(e.rc==143)throw Error("cancelled",71);throw;}}
static void readonly(bool change){
    unsigned count=0;unsigned long flags=MS_REMOUNT|MS_RDONLY;bool ro=false;
    std::istringstream rows(read("/proc/self/mounts",1024*1024));std::string line;
    while(std::getline(rows,line)){std::istringstream row(line);std::string src,path,type,opts;require(bool(row>>src>>path>>type>>opts),"stack-mount-record",90);if(path!="/system")continue;count++;
        std::istringstream items(opts);std::string item;while(std::getline(items,item,',')){
            if(item=="ro")ro=true;else if(item=="nosuid")flags|=MS_NOSUID;else if(item=="nodev")flags|=MS_NODEV;else if(item=="noexec")flags|=MS_NOEXEC;else if(item=="sync")flags|=MS_SYNCHRONOUS;else if(item=="dirsync")flags|=MS_DIRSYNC;else if(item=="noatime")flags|=MS_NOATIME;else if(item=="nodiratime")flags|=MS_NODIRATIME;else if(item=="relatime")flags|=MS_RELATIME;else if(item=="strictatime")flags|=MS_STRICTATIME;
        }
    }
    require(count==1,"stack-system-mount-ambiguous",90);
    if(change){if(mount(nullptr,"/system",nullptr,flags,nullptr))throw Error("stack-system-readonly",90,true,errno);readonly(false);}else require(ro,"stack-system-not-readonly",90);
}
static void live(bool on){auto p=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.set_live_view",on?"boolean:true":"boolean:false"});require(!p.rc,"stack-live",p.rc);}
static void put(const std::string&name,const std::string&expected){
    require(name=="drive_mode"||name=="multishot_control_mode"||name=="exposures_in_multishot_session"||name=="image_format","stack-setting-name",2);
    auto current=get("camera",name,"int32");require(digits(current)&&digits(expected),"stack-setting-value");
    if(current!=expected){auto p=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=3000","--dest=com.hasselblad.camera","/camera","org.freedesktop.DBus.Properties.Set","string:com.hasselblad.camera","string:"+name,"variant:int32:"+expected});require(!p.rc,"stack-setting-write",p.rc);}
    require(get("camera",name,"int32")==expected,"stack-setting-verify");
}
// get adds 1000 ms for child cleanup; the command executor accepts <=180000.
static void poll_hook(){get("storage","capture_storage_mode","int32",179000);}
static void pump(){try{poll_hook();}catch(const Error&e){if(e.rc==71||e.rc==143)throw;}catch(const std::exception&){} }
static bool idle(){return get("camera","exposure_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("storage","storage_status","int32")=="0";}
struct AdmissionState {std::string exposure,error,storage;};
// hook.ready proves the receiver binding, not that the replacement storage
// service has finished its native startup. Never reinterpret a nonzero status
// as idle; wait for three consecutive idle observations before publishing ready
// and again after the user's trigger. Reads and ownership failures propagate.
template<class Read,class Now,class Pause,class Check>
static AdmissionState await_owned_idle(Read read_state,Now now,Pause pause,Check check){
 const auto begin=now();unsigned stable=0;std::string previous;
 for(;;){
  check();require(now()-begin<5000,"stack-storage-ready-timeout",19);
  const auto state=read_state();
  require(digits(state.exposure)&&digits(state.storage)&&(state.error=="true"||state.error=="false"),"stack-admission-state",19);
  require(state.error=="false","stack-native-error",19);
  require(now()-begin<5000,"stack-storage-ready-timeout",19);
  stable=state.exposure=="0"&&state.storage=="0"?stable+1:0;
  const auto current=state.exposure+" "+state.error+" "+state.storage;
  if(current!=previous){fprintf(stderr,"STACK_ADMISSION_WAIT exposure_status=%s active_error=%s storage_status=%s\n",state.exposure.c_str(),state.error.c_str(),state.storage.c_str());previous=current;}
  if(stable>=3)return state;
  pause(100);
 }
}
static bool mapped_module_identity(const std::string&maps,const struct stat&st,const std::string&path=module);
static Identity original_storage(){
    auto ps=pids("camera-storage");require(ps.size()==1,"stack-original-storage-count",22);auto id=identify(ps[0]);
    require(executable(id.pid)=="/system/bin/camera-storage","stack-original-storage-exe",22);
    auto prefix="/proc/"+std::to_string(id.pid);auto env=records(read(prefix+"/environ",131072));std::vector<std::string> loader;for(const auto&line:env)if(line.rfind("LD_",0)==0)loader.push_back(line);
    auto maps=read(prefix+"/maps",2097152);require(!maps.empty()&&maps.find(module)==std::string::npos,"stack-original-storage-maps",22);
    if(!loader.empty()){
        require(loader.size()==1&&loader[0]=="LD_PRELOAD=/system/lib64/libx2d_quarter_native.so","stack-original-storage-loader",22);
        fixed_hash("/system/lib64/libx2d_quarter_native.so",quarter_hash);
        struct stat native;require(!lstat("/system/lib64/libx2d_quarter_native.so",&native)&&S_ISREG(native.st_mode)&&native.st_nlink==1,"stack-quarter-module-identity",22);
        require(value("/tmp/x2d-quarter-native.status")=="QUARTER_NATIVE_READY"&&value("/tmp/x2d-pixel-overclock/quarter.txt")=="DISARMED"&&mapped_module_identity(maps,native,"/system/lib64/libx2d_quarter_native.so"),"stack-quarter-state",22);
    }
    fixed_hash("/system/bin/camera-storage",storage_hash);require(pids("camera-storage")==ps&&identify(id.pid).start==id.start&&executable(id.pid)=="/system/bin/camera-storage","stack-original-storage-changed",22);return id;
}
static void stable_original(){
    auto begin=monotonic_ms();unsigned stable=0;std::string key;
    for(unsigned i=0;i<25;i++){try{auto id=original_storage();auto now=std::to_string(id.pid)+":"+id.start;if(key==now)stable++;else{key=now;stable=1;}}catch(const Error&e){if(e.rc==143||e.rc==71)throw;stable=0;key.clear();}
        require(monotonic_ms()-begin<5000,"stack-original-storage-stability",90);if(stable>=3)return;delay(100);
    }throw Error("stack-original-storage-stability",90);
}
static bool mapped_module_identity(const std::string&maps,const struct stat&st,const std::string&path){
 std::istringstream rows(maps);std::string row;bool found=false;
 while(std::getline(rows,row)){std::istringstream fields(row);std::string address,perms,offset,dev,inode,name;require(bool(fields>>address>>perms>>offset>>dev>>inode),"stack-maps-record",90);std::getline(fields,name);auto at=name.find_first_not_of(' ');if(at==std::string::npos||name.substr(at)!=path)continue;
  auto colon=dev.find(':');require(colon!=std::string::npos&&colon>0&&colon+1<dev.size()&&digits(inode)&&inode.size()<=20,"stack-maps-identity-fields",90);auto maj=dev.substr(0,colon),min=dev.substr(colon+1);require(maj.size()<=8&&min.size()<=8&&maj.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos&&min.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"stack-maps-device",90);
  require(std::stoull(inode)==uint64_t(st.st_ino)&&std::stoull(maj,nullptr,16)==uint64_t(major(st.st_dev))&&std::stoull(min,nullptr,16)==uint64_t(minor(st.st_dev)),"stack-maps-module-inode",90);found=true;
 }return found;
}
static void owned_storage(const std::string&s,pid_t p){
    auto start=value(s+"/storage.start");require(digits(start)&&identify(p).start==start&&executable(p)=="/system/bin/camera-storage","stack-owned-storage-identity",90);
    fixed_hash(module,module_hash);struct stat mapped;require(!lstat(module.c_str(),&mapped)&&S_ISREG(mapped.st_mode)&&mapped.st_nlink==1,"stack-module-identity",90);
    auto prefix="/proc/"+std::to_string(p);require(mapped_module_identity(read(prefix+"/maps",2097152),mapped),"stack-owned-storage-module",90);
    require(contains(records(read(prefix+"/environ",131072)),"X2D_RAW_STACK_JOB="+s),"stack-owned-storage-job",90);
    require(identify(p).start==start&&executable(p)=="/system/bin/camera-storage","stack-owned-storage-changed",90);
}
static void wait_storage_exit(pid_t p,const std::string&expected){
    // 恢复可能在 Runner 本身或其 sibling helper 内执行；只有维护 ProcessChild 的 Runner 可以 waitpid。
    // 此处只观察退出/僵尸并核对 incarnation，不消费其他所有者的子进程状态。
    for(unsigned i=0;alive(p);i++){auto id=identify(p);require(id.start==expected,"stack-storage-pid-reused",90);if(id.state=='Z')break;require(i<49,"stack-owned-storage-not-exited",90);delay(100);}
}
struct Settings {std::string drive,mode,frames,format;};
static void validate(const Settings&v){require(v.drive.size()==1&&v.drive[0]>='0'&&v.drive[0]<='6'&&v.mode.size()==1&&v.mode[0]>='0'&&v.mode[0]<='3'&&(v.frames=="1"||v.frames=="4"||v.frames=="6"||v.frames=="16")&&v.format.size()==1&&v.format[0]>='0'&&v.format[0]<='4',"stack-snapshot-rejected",20);}
static Settings settings(){Settings v{get("camera","drive_mode","int32"),get("camera","multishot_control_mode","int32"),get("camera","exposures_in_multishot_session","int32"),get("camera","image_format","int32")};validate(v);return v;}
static Settings snapshot(const std::string&s){Settings v{value(s+"/drive.before"),value(s+"/mode.before"),value(s+"/frames.before"),value(s+"/format.before")};validate(v);return v;}
static void save_settings(const std::string&s,const Settings&v,bool initial){
    std::array<std::pair<const char*,std::string>,4> items={{{"drive",v.drive},{"mode",v.mode},{"frames",v.frames},{"format",v.format}}};
    for(const auto&i:items){auto path=s+"/"+i.first+".before";if(initial)write_new(path,i.second+"\n");else{require(regular(path),"stack-snapshot-type");write_replace(path,i.second+"\n");}}
}
struct DirOwner {
    std::string path;struct stat id{};bool held=false;int fd=-1;
    explicit DirOwner(std::string p):path(std::move(p)){}
    ~DirOwner(){if(fd>=0)close(fd);} // Failed ownership checks never remove a path.
    DirOwner(const DirOwner&)=delete;DirOwner&operator=(const DirOwner&)=delete;
    void pin(){
     require(held&&fd<0,"stack-lock-pin-state",90);fd=open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
     if(fd<0)throw Error("stack-lock-open",90,true,errno);
     require(!fstat(fd,&id)&&S_ISDIR(id.st_mode)&&id.st_ino,"stack-lock-identity",90);
     struct stat current;require(!lstat(path.c_str(),&current)&&S_ISDIR(current.st_mode)&&current.st_dev==id.st_dev&&current.st_ino==id.st_ino,"stack-lock-pin-replaced",90);
    }
    void acquire(){mkdir_new(path);held=true;pin();}
    void release(){
     // exFAT synthesizes inode numbers for cached inodes. Keep this directory
     // open from acquisition through release, instead of comparing two
     // unpinned lookups across storage restart and memory pressure.
     struct stat pinned{},now{};bool own=held&&fd>=0&&!fstat(fd,&pinned)&&!lstat(path.c_str(),&now)&&S_ISDIR(now.st_mode)&&pinned.st_dev==id.st_dev&&pinned.st_ino==id.st_ino&&now.st_dev==id.st_dev&&now.st_ino==id.st_ino;
     if(!own)std::fprintf(stderr,"STACK_LOCK_IDENTITY_MISMATCH held=%d fd_present=%d expected_dev=%llu expected_ino=%llu pinned_ino=%llu current_ino=%llu errno=%d\n",held,fd>=0,(unsigned long long)id.st_dev,(unsigned long long)id.st_ino,(unsigned long long)pinned.st_ino,(unsigned long long)now.st_ino,errno);
     require(own,"stack-lock-replaced",90);if(rmdir(path.c_str()))throw Error("stack-unlock",90,true,errno);held=false;close(fd);fd=-1;
    }
};
struct ProcessChild {
    pid_t pid=-1;std::string start;bool done=false;int rc=125;
    bool poll(){if(done)return true;require(pid>0,"stack-child-not-started");int status;pid_t got;do{got=waitpid(pid,&status,WNOHANG);}while(got<0&&errno==EINTR);require(got>=0,"stack-child-poll");if(got==pid){done=true;rc=WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?128+WTERMSIG(status):1;}return done;}
    int wait(){while(!poll())delay(20);return rc;}
    void launch(const std::string&log,const std::function<int()>&body,const std::function<void()>&registered={},bool discard_output=false){
        local_cancel();require(pid<0,"stack-child-reused",2);int output=discard_output?open("/dev/null",O_WRONLY|O_CLOEXEC):open(log.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);require(output>=0,"stack-child-log");
        int original_pipe[2];if(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,original_pipe)){int cause=errno;close(output);throw Error("stack-child-gate",1,false,cause);}
        int gate_read=fcntl(original_pipe[0],F_DUPFD_CLOEXEC,3),gate_write=fcntl(original_pipe[1],F_DUPFD_CLOEXEC,3);int gate_errno=errno;close(original_pipe[0]);close(original_pipe[1]);
        if(gate_read<0||gate_write<0){if(gate_read>=0)close(gate_read);if(gate_write>=0)close(gate_write);close(output);throw Error("stack-child-gate-fd",1,false,gate_errno);}
        fflush(nullptr);pid=fork();int cause=errno;
        if(pid<0){close(output);close(gate_read);close(gate_write);throw Error("stack-child-fork",1,false,cause);}
        if(!pid){
            close(gate_write);char permission=0;ssize_t got;do{got=::read(gate_read,&permission,1);}while(got<0&&errno==EINTR);close(gate_read);if(got!=1||permission!='G')_exit(126);
            int copy=fcntl(output,F_DUPFD_CLOEXEC,3);if(copy<0||dup2(copy,1)<0||dup2(copy,2)<0)_exit(126);close(copy);if(output!=1&&output!=2)close(output);
            int input=open("/dev/null",O_RDONLY|O_CLOEXEC);if(input<0||dup2(input,0)<0)_exit(126);if(input!=0)close(input);
            int result=1;try{result=body();}catch(const Error&e){int primary=e.rc==143?71:e.rc;result=e.recovery_failed?90:primary;fprintf(stderr,"MEMORY_CHILD_FAILURE step=%s primary_rc=%d restore_rc=%d errno=%d\n",e.what(),primary,e.recovery_failed?90:0,e.system_errno);}catch(const std::exception&e){fprintf(stderr,"MEMORY_CHILD_FAILURE detail=%s\n",e.what());}
            fflush(nullptr);_exit(result);
        }
        close(output);close(gate_read);
        try{start=identify(pid).start;local_cancel();if(registered)registered();local_cancel();char permission='G';ssize_t sent;do{sent=send(gate_write,&permission,1,MSG_NOSIGNAL);}while(sent<0&&errno==EINTR);require(sent==1,"stack-child-gate-release");close(gate_write);}
        catch(...){close(gate_write);throw;}
    }
};
struct Config {unsigned method,seconds,noise,delay,target_iso;};
static unsigned number(const std::string&s,unsigned low,unsigned high){require(digits(s)&&s.size()<=10&&(s=="0"||s[0]!='0'),"stack-number",2);auto n=std::stoull(s);require(n>=low&&n<=high,"stack-number-range",2);return unsigned(n);}
static Config config(const std::string&m,const std::string&seconds,const std::string&noise,const std::string&delay){
 Config c{number(m,0,3),0,number(noise,0,0),number(delay,0,60),0};
 if(c.method==3){c.target_iso=number(seconds,1,65535);c.method=2;c.seconds=86399;}else c.seconds=number(seconds,1,86399);return c;
}
struct IsoPlan {unsigned source=0,target=0,frames=0;uint32_t numerator=0,denominator=0;};
static IsoPlan parse_iso(const std::string&text,unsigned target){
 std::istringstream in(text);std::string tag,a,b,c,d,e,extra;require(bool(in>>tag>>a>>b>>c>>d>>e)&&!(in>>extra)&&tag=="RSIP1","stack-iso-plan",26);
 IsoPlan plan{number(a,1,65535),number(b,1,65535),number(c,1,65535),number(d,1,UINT_MAX),number(e,1,UINT_MAX)};
 require(text=="RSIP1 "+a+" "+b+" "+c+" "+d+" "+e+"\n"&&plan.target==target&&plan.target<=plan.source&&plan.frames==(plan.source+plan.target-1)/plan.target,"stack-iso-plan-canonical",26);return plan;
}
static bool continue_frames(unsigned count,uint64_t now,uint64_t deadline,bool finished,const Config&cfg,const IsoPlan&plan){return count<65535&&!finished&&(cfg.target_iso?(!count||count<plan.frames):now<deadline);}
static int invoke(int(*entry)(int,char**),std::vector<std::string> args){std::vector<char*> av;for(auto&a:args)av.push_back(a.data());return entry(int(av.size()),av.data());}
static void native_result(int rc,const char*why){if(rc)throw Error(why,rc,rc==90);}
enum class Tool {Inputs,Storage,Render,Finalize,Pack,Heif};
static int exec_tool(Tool t,const std::vector<std::string>&extra,const std::string&s={}){
 const char*p=t==Tool::Inputs?"/system/bin/x2d-pixel-overclock/inputs":t==Tool::Storage?"/system/bin/camera-storage":t==Tool::Render?"/system/bin/x2d-pixel-overclock/render-probe":t==Tool::Finalize?"/system/bin/x2d-raw-stack/finalize":t==Tool::Pack?"/system/bin/x2d-raw-stack/stack-pack":"/system/bin/x2d-pixel-overclock/heif";
 const char*digest=t==Tool::Inputs?runtime::pixel_inputs_sha:t==Tool::Storage?storage_hash:t==Tool::Render?"1355b962f46ba01ee511dd436aa25d8cfda73c21d41d75a6bd0c496bd176f846":t==Tool::Finalize?"3c86b4eab2dcf890ee10f911a9a3c55be8f810f39ebb60dba47acadbccea37f7":t==Tool::Pack?"9f7ce45aa0b937d26a0bf4cef0abca811924fbc76af018325ad6c1dc4358b710":"41f17819e58831d8d60a362ce17f54b49e4dc0f0e061ec940bff9608ab4f2d4a";fixed_hash(p,digest);
 std::vector<std::string> args={p};args.insert(args.end(),extra.begin(),extra.end());std::vector<char*> av;for(auto&a:args)av.push_back(a.data());av.push_back(nullptr);
 std::vector<std::string> env;for(char**e=environ;e&&*e;e++){std::string line=*e;if(line.rfind("LD_",0)==0||line.rfind("X2D_",0)==0)continue;env.push_back(line);}
 if(t==Tool::Storage){require(extra.empty()&&!s.empty(),"stack-storage-contract",2);fixed_hash(module,module_hash);env.push_back("LD_PRELOAD="+module);env.push_back("X2D_RAW_STACK_ABI=1");env.push_back("X2D_RAW_STACK_JOB="+s);}
 std::vector<char*> ev;for(auto&e:env)ev.push_back(e.data());ev.push_back(nullptr);signal(SIGHUP,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);execve(p,av.data(),ev.data());return 127;
}
static std::string raw_destination(const std::string&s){auto plan=album_plan(value(s+"/output-plan",128));return "/mnt/media_rw/ssd/DCIM/"+plan.folder+"/"+plan.image+".3FR";}
static struct stat raw_stat(const std::string&p){struct stat st;require(!lstat(p.c_str(),&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_size>0&&st.st_size%4096==0,"stack-raw-not-durable",90);return st;}
static std::string raw_binding(const std::string&s,const struct stat&st){return "STACK_RELEASED_RAW_PUBLICATION_V1\njob="+s+"\ntarget="+raw_destination(s)+"\ndev="+std::to_string(uint64_t(st.st_dev))+"\nino="+std::to_string(uint64_t(st.st_ino))+"\nsize="+std::to_string(uint64_t(st.st_size))+"\n";}
static std::string raw_content_binding(const std::string&s,const std::string&p,const struct stat&st){return "STACK_RELEASED_RAW_CONTENT_V1\njob="+s+"\ntarget="+raw_destination(s)+"\nsize="+std::to_string(uint64_t(st.st_size))+"\nsha256="+sha256(p)+"\n";}
static void durable_raw(const std::string&s){
 // 保留旧 inode 凭据；exFAT 复制发布以发布前绑定的内容摘要验证，仍要求独立释放凭据。
 const bool staged=exists(s+"/merged.3FR"),bound=exists(s+"/publish.raw.identity");require(staged||bound,"stack-missing-raw-without-publication-binding",90);
 auto st=raw_stat(staged?s+"/merged.3FR":raw_destination(s));
 if(bound){
  require(regular(s+"/publish.raw.identity"),"stack-publication-binding-type",90);
  if(exists(s+"/publish.raw.content"))require(regular(s+"/publish.raw.content")&&read(s+"/publish.raw.content",1024)==raw_content_binding(s,staged?s+"/merged.3FR":raw_destination(s),st),"stack-published-raw-content",90);
  else require(read(s+"/publish.raw.identity",1024)==raw_binding(s,st),"stack-published-raw-identity",90);
 }
}
static void render_drained(const std::string&s){
 std::istringstream lines(read(s+"/probe.log",16*1024*1024));std::string line;unsigned found=0;
 while(std::getline(lines,line)){if(line.rfind("RAW_STACK_RENDER_EXIT ",0)!=0)continue;found++;std::istringstream fields(line);std::string tag,primary,final,cleanup,extra;require(bool(fields>>tag>>primary>>final>>cleanup)&&!(fields>>extra)&&primary.rfind("primary_rc=",0)==0&&final.rfind("final_rc=",0)==0&&cleanup=="cleanup_rc=0","stack-render-release-receipt",90);number(primary.substr(11),0,255);number(final.substr(9),0,255);}
 require(found==1&&get("camera","stream_group","int32")=="255","stack-render-release-unconfirmed",90);
}
static bool natural_drained(const std::string&s){
 if(!exact(s+"/raw.ready","RAW_STACK_R1"))return false;
 require(log_count(s+"/capture.log","RAW_STACK_FINAL_RAW_WRITTEN_ALL_SOURCE_REFERENCES_RELEASED")==1,"stack-release-log",90);durable_raw(s);return true;
}
static void bind_raw_publication(const std::string&s){
 // 在 finalizer 改写完成后、publisher 第一次 rename 前绑定；publisher 提交后索引失败也可以核验成片。
 require(natural_drained(s),"stack-publication-before-drain",90);auto st=raw_stat(s+"/merged.3FR");write_new(s+"/publish.raw.content",raw_content_binding(s,s+"/merged.3FR",st));write_new(s+"/publish.raw.identity",raw_binding(s,st));durable_raw(s);
}
static bool aborted_drained(const std::string&s){if(!exact(s+"/abort.drained","RAW_STACK_ABORT_DRAINED_V1"))return false;require(log_count(s+"/capture.log","RAW_STACK_ABORT_ALL_SOURCE_REFERENCES_AND_POOL_RELEASED")==1,"stack-abort-release-log",90);return true;}
static bool recorded_rpc_finished(const std::string&s){
 if(!exists(s+"/capture.rpc.pid"))return true;
 auto p=parse_pid(value(s+"/capture.rpc.pid"));if(!alive(p))return true;
 auto id=identify(p);require(id.start==value(s+"/capture.rpc.start"),"stack-burst-rpc-reused",90);return id.state=='Z';
}
static void stop_continuous(const std::string&s){
 if(!exists(s+"/burst.active"))return;
 auto epoch=value(s+"/burst.active");require(digits(epoch)&&epoch.size()<=10&&epoch!="0","stack-burst-epoch",90);
 auto token="RSB1 "+epoch;if(exact(s+"/burst.stopped",token))return;
 // 先禁止继续请求，再等待曝光、原厂通知及本次 RPC 全部结束。不能只凭 stop 返回判断收尾。
 auto deadline=monotonic_ms()+190000;uint64_t last_stop=0;unsigned stable=0;
 for(;;){
  const auto exposure=get("camera","exposure_status","int32"),storage_state=get("storage","storage_status","int32");
  if(exposure=="0"&&storage_state=="0"&&recorded_rpc_finished(s)){
   if(++stable==3)break;
  }else{
   stable=0;auto now=monotonic_ms();
   if(!last_stop||now-last_stop>=1000){
    auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.stop_exposing"});
    require(!r.rc,"stack-burst-stop-rejected",90);last_stop=now;
   }
  }
  require(monotonic_ms()<deadline,"stack-burst-stop-unconfirmed",90);pump();delay(20);
 }
 receipt(s,"burst.closed",token);receipt(s,"burst.stopped",token);sync();
}
static void rpc_idle(const std::string&s){
 if(!exists(s+"/capture.rpc.pid"))return;
 auto p=parse_pid(value(s+"/capture.rpc.pid"));auto expected=value(s+"/capture.rpc.start");require(digits(expected),"stack-rpc-start",90);
 for(unsigned i=0;alive(p);i++){auto id=identify(p);require(id.start==expected,"stack-rpc-reused",90);if(id.state=='Z')break;require(i<999,"stack-rpc-still-active",90);delay(200);}
}
static void drain_stack(const std::string&s){
 stop_continuous(s);
 rpc_idle(s);
 if(natural_drained(s)||aborted_drained(s))return;
 if(!exists(s+"/storage.pid")){require(!exists(s+"/frame.request")&&!exists(s+"/capture.invoked"),"stack-no-storage-with-active-frame",90);return;}
 auto p=parse_pid(value(s+"/storage.pid"));owned_storage(s,p);
 require(exact(s+"/abort.supported","RAW_STACK_ABORT_PROTOCOL_V1"),"stack-abort-protocol-unavailable",90);
 // 取消先等已经提交的单帧自然结束；不再提交下一帧，不杀持有 RAW 的服务。
 for(unsigned i=0;get("camera","exposure_status","int32")!="0"||get("storage","storage_status","int32")!="0";i++){require(i<180,"stack-abort-native-not-idle",90);pump();delay(1000);}
 receipt(s,"abort.request","RAW_STACK_ABORT_IDLE_V1");sync();
 for(unsigned i=0;!exact(s+"/abort.drained","RAW_STACK_ABORT_DRAINED_V1");i++){owned_storage(s,p);require(i<599,"stack-abort-drain-unconfirmed",90);pump();delay(100);}
 require(aborted_drained(s),"stack-abort-final-proof",90);
}
static void restored_check(const std::string&s){
 require(exact(s+"/restored","RESTORED_STACK_V1")&&exact(s+"/restore.rc","0")&&exact(s+"/restore.phase","COMPLETE"),"stack-restore-markers",90);readonly(false);stable_original();
 if(exists(s+"/frame.request")||exists(s+"/capture.invoked"))require(natural_drained(s)||aborted_drained(s),"stack-restored-without-drain",90);
}
static int restore(const std::string&s){
 Recovery recovery;if(exists(s+"/restored")){restored_check(s);return 0;}DirOwner lock(s+"/restore.lock");bool marker=false;struct stat marker_id{};int rc=0;
 for(unsigned i=0;;i++){if(!mkdir(lock.path.c_str(),0700)){lock.held=true;lock.pin();break;}require(errno==EEXIST&&directory(lock.path),"stack-restore-lock",90);if(exists(s+"/restored")){restored_check(s);return 0;}require(i<29,"stack-restore-lock-busy",90);delay(1000);}
 try{
  receipt(s,"restore.rc","125");receipt(s,"restore.phase","RAW_DRAIN");drain_stack(s);
  receipt(s,"restore.phase","SETTINGS");bool settings_failed=false;
  if(exists(s+"/settings.modified")){require(regular(s+"/settings.modified"),"stack-settings-marker",90);auto v=snapshot(s);std::array<std::pair<const char*,std::string>,4> items={{{"drive_mode",v.drive},{"multishot_control_mode",v.mode},{"exposures_in_multishot_session",v.frames},{"image_format",v.format}}};for(const auto&i:items){try{put(i.first,i.second);}catch(const std::exception&e){settings_failed=true;fprintf(stderr,"STACK_RESTORE_SETTING property=%s error=%s\n",i.first,e.what());}}}
  receipt(s,"restore.phase","OWNED_STORAGE");if(exists(s+"/storage.pid")){auto p=parse_pid(value(s+"/storage.pid"));if(alive(p)){auto id=identify(p);auto expected=value(s+"/storage.start");require(id.start==expected,"stack-storage-reused",90);if(id.state!='Z'){owned_storage(s,p);require(!kill(p,SIGTERM)||errno==ESRCH,"stack-storage-term",90);}wait_storage_exit(p,expected);}}
  readonly(true);fixed_hash(module,module_hash);receipt(s,"restore.phase","ORIGINAL_STORAGE");if(exists(s+"/storage.stopped")){require(regular(s+"/storage.stopped"),"stack-stop-marker",90);service("camera-storage",true);}stable_original();require(!settings_failed,"stack-settings-restore",90);
  // 此阶段恢复存储但不恢复 live；raw-stack render 仍需暂停取景，最终 cleanup 恢复两个原始状态。
  readonly(true);sync();receipt(s,"restore.phase","COMPLETE");receipt(s,"restore.rc","0");write_new(s+"/restored","RESTORED_STACK_V1\n");marker=true;require(!lstat((s+"/restored").c_str(),&marker_id),"stack-restored-identity",90);sync();
 }catch(const Error&e){rc=e.rc;fprintf(stderr,"STACK_RESTORE_PRIMARY step=%s primary_rc=%d errno=%d\n",e.what(),e.rc,e.system_errno);}catch(const std::exception&e){rc=90;fprintf(stderr,"STACK_RESTORE_PRIMARY detail=%s\n",e.what());}
 try{readonly(true);}catch(const Error&e){rc=90;fprintf(stderr,"STACK_RESTORE_FINAL_READONLY step=%s errno=%d\n",e.what(),e.system_errno);}catch(const std::exception&e){rc=90;fprintf(stderr,"STACK_RESTORE_FINAL_READONLY detail=%s\n",e.what());}
 try{lock.release();}catch(const Error&e){rc=90;fprintf(stderr,"STACK_RESTORE_UNLOCK step=%s errno=%d\n",e.what(),e.system_errno);}catch(const std::exception&e){rc=90;fprintf(stderr,"STACK_RESTORE_UNLOCK detail=%s\n",e.what());}
 if(rc&&marker){try{struct stat current;require(marker_id.st_ino&&!lstat((s+"/restored").c_str(),&current)&&current.st_dev==marker_id.st_dev&&current.st_ino==marker_id.st_ino,"stack-restored-replaced",90);remove_regular(s+"/restored");}catch(const std::exception&){rc=90;}}
 try{receipt(s,"restore.rc",std::to_string(rc));}catch(const std::exception&){rc=90;}return rc;
}
static void owner_identity(const std::string&s,pid_t owner){
 require(identify(owner).start==value(s+"/run.start"),"stack-owner-start",70);struct stat self,other;require(!stat("/proc/self/exe",&self)&&!stat(("/proc/"+std::to_string(owner)+"/exe").c_str(),&other)&&self.st_dev==other.st_dev&&self.st_ino==other.st_ino,"stack-owner-exe",70);auto args=records(read("/proc/"+std::to_string(owner)+"/cmdline",16384));require(args.size()>=3&&args[1]=="stack-run"&&args[2]==s,"stack-owner-command",70);
}
static int watchdog(const std::string&s,pid_t owner){
 Recovery recovery;auto deadline=monotonic_ms()+90000000;auto current=[&](){if(!alive(owner))return false;auto id=identify(owner);if(id.state=='Z'||id.start!=value(s+"/run.start"))return false;owner_identity(s,owner);return true;};
 while(!exists(s+"/run.rc")&&current()&&monotonic_ms()<deadline)delay(1000);
 if(!exists(s+"/run.rc")){if(current()){owner_identity(s,owner);require(!kill(owner,SIGTERM)||errno==ESRCH,"stack-watchdog-term",70);delay(2000);}return restore(s);}return 0;
}
struct Runner {
 std::string s,stage_name="preflight",phase="LOCK",live_before,shutter,previous_progress;Config cfg;IsoPlan iso;DirOwner lock;ProcessChild storage,capture,utility,watch;unsigned count=0;uint64_t gap=0,deadline=0,next_error=0;bool armed=false,recovery_problem=false;
 Runner(std::string path,Config c):s(std::move(path)),cfg(c),lock(global_lock){}
 void stage(const char*name){stage_name=name;receipt(s,"stage",name);}
 void alive_local(){require(!storage.poll()&&!exists(s+"/failed"),"stack-capture-local-failure",25);}
 void alive_current(){alive_local();auto now=monotonic_ms();if(now>=next_error){require(get("error","active_error","boolean")=="false","stack-native-error",25);next_error=now+250;}}
 void capture_result(){if(exists(s+"/capture.rc"))require(exact(s+"/capture.rc","0"),"stack-exposure-request-rejected",27);if(capture.pid>0&&capture.poll())require(!capture.rc,"stack-exposure-rpc-failed",27);}
 void read_plan(){if(!cfg.target_iso||iso.frames)return;if(!exists(s+"/iso.plan")){require(!count,"stack-iso-plan-missing",26);return;}iso=parse_iso(read(s+"/iso.plan",128),cfg.target_iso);receipt(s,"iso.progress",std::to_string(iso.source)+" "+std::to_string(iso.target)+" "+std::to_string(iso.frames)+" "+std::to_string(iso.numerator)+" "+std::to_string(iso.denominator));}
 void progress(){unsigned accumulated=0;if(exists(s+"/frame.done")){auto text=read(s+"/frame.done",64);require(text.rfind("RS1 ",0)==0&&text.back()=='\n',"stack-frame-done",26);accumulated=number(text.substr(4,text.size()-5),0,65535);}auto now=monotonic_ms();auto left=now<deadline?(deadline-now+999)/1000:0;auto current=std::to_string(accumulated)+" "+std::to_string(gap)+" "+std::to_string(left);if(current!=previous_progress){receipt(s,"stack.progress",current);previous_progress=current;}read_plan();}
 uint64_t wait_token(const char*name,const std::string&wanted){auto end=monotonic_ms()+180000;for(;;){local_cancel();alive_local();capture_result();if(exact(s+"/"+name,wanted))return end;alive_current();poll_hook();if(exact(s+"/"+name,wanted))return end;require(monotonic_ms()<end,"stack-frame-wait-timeout",26);progress();delay(10);}}
 void tool(Tool t,const std::vector<std::string>&args,const char*log){require(utility.pid<0||utility.done,"stack-tool-overlap",90);utility=ProcessChild{};utility.launch(s+"/"+log,[&](){return exec_tool(t,args);});native_result(utility.wait(),"stack-tool");}
 void idle_admission(bool wait_owned=false){
  auto read_state=[](){return AdmissionState{get("camera","exposure_status","int32"),get("error","active_error","boolean"),get("storage","storage_status","int32")};};
  AdmissionState state;
  if(wait_owned){
   owned_storage(s,storage.pid);
   state=await_owned_idle(read_state,[](){return monotonic_ms();},[](unsigned ms){delay(ms);},[&](){local_cancel();alive_local();cancel_before_capture(s);});
   owned_storage(s,storage.pid);
  }else state=read_state();
  const auto &exposure=state.exposure,&error=state.error,&storage_status=state.storage;
  bool admitted=exposure=="0"&&error=="false"&&storage_status=="0";
  if(!admitted)std::fprintf(stderr,"STACK_ADMISSION_REJECT phase=%s exposure_status=%s active_error=%s storage_status=%s\n",phase.c_str(),exposure.c_str(),error.c_str(),storage_status.c_str());
  require(admitted,"stack-not-idle",19);auto current=get("camera","eshutter_current","boolean");require((current=="true"||current=="false")&&(shutter.empty()||current==shutter),"stack-shutter-state",19);require(get("storage","capture_storage_mode","int32")=="1","stack-local-storage",19);auto depth=get("camera","image_bit_depth","int32");require(depth=="0"||depth=="1","stack-bit-depth",19);shutter=current;
 }
 void burst_progress(unsigned epoch){
  if(!exists(s+"/burst.progress")){progress();return;}
  std::istringstream fields(read(s+"/burst.progress",128));std::string tag,a,b,c,d,extra;
  require(bool(fields>>tag>>a>>b>>c>>d)&&!(fields>>extra)&&tag=="RSB1","stack-burst-progress",26);
  auto generation=number(a,1,UINT_MAX),accepted=number(b,1,65535),peak=number(d,0,3);
  require(generation<=epoch&&accepted>=count,"stack-burst-progress-order",26);
  count=accepted;gap=number(c,0,UINT_MAX);(void)peak;progress();
 }
 void continuous(){
  for(unsigned epoch=1;continue_frames(count,monotonic_ms(),deadline,exists(s+"/finish.request"),cfg,iso);epoch++){
   const auto segment_begin=monotonic_ms();const unsigned count_before=count;
   require(epoch<UINT_MAX,"stack-burst-epoch-limit",26);local_cancel();alive_current();require(idle(),"stack-burst-not-idle",19);
   std::string token="RSB1 "+std::to_string(epoch);receipt(s,"burst.request",token);wait_token("burst.armed",token);
   const auto armed_at=monotonic_ms();
   remove_if(s+"/capture.rc");require(capture.pid<0||capture.done,"stack-burst-rpc-overlap",90);capture=ProcessChild{};
   receipt(s,"burst.active",std::to_string(epoch));if(!exists(s+"/capture.invoked"))write_new(s+"/capture.invoked","");
   capture.launch({},[&](){Recovery detached;auto result=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=180000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.do_exposure_extended2","boolean:true","int32:8","int32:180000"},180000,false);receipt(s,"capture.rc",std::to_string(result.rc));write_replace(s+"/capture-request.log",result.output);sync();return result.rc;},[&](){receipt(s,"capture.rpc.start",capture.start);receipt(s,"capture.rpc.pid",std::to_string(capture.pid));sync();},true);
   auto segment_end=monotonic_ms()+30000;bool pressure=false;
   const auto submitted_at=monotonic_ms();
   for(;;){
    local_cancel();alive_current();poll_hook();capture_result();burst_progress(epoch);
    pressure=exact(s+"/burst.pressure",token);
    if(pressure||exists(s+"/finish.request")||monotonic_ms()>=deadline||monotonic_ms()>=segment_end||count>=65533)break;
    require(!capture.poll(),"stack-burst-ended-without-stop",27);delay(10);
   }
   const auto stop_begin=monotonic_ms();
   stop_continuous(s);const auto stopped_at=monotonic_ms();
   native_result(capture.wait(),"stack-burst-rpc");capture_result();wait_token("burst.drained",token);burst_progress(epoch);
   const auto drained_at=monotonic_ms();
   printf("RAW_STACK_BURST_TIMING epoch=%u frames=%u arm_ms=%llu submit_ms=%llu active_ms=%llu stop_ms=%llu drain_ms=%llu total_ms=%llu\n",epoch,count-count_before,
    (unsigned long long)(armed_at-segment_begin),(unsigned long long)(submitted_at-armed_at),
    (unsigned long long)(stop_begin-submitted_at),(unsigned long long)(stopped_at-stop_begin),
    (unsigned long long)(drained_at-stopped_at),(unsigned long long)(drained_at-segment_begin));
   printf("RAW_STACK_BURST_DRAINED epoch=%u frames=%u pressure_stop=%u\n",epoch,count,unsigned(pressure));
   // 65535 是数值硬上限，65533 提前请求停止；异常超额会在接收端失败，不产生伪成功成片。
   if(count>=65533)break;
  }
 }
 int cleanup(int primary){
  Recovery recovery;int restore_rc=125,live_rc=125,lock_rc=125;bool problem=recovery_problem;
  if(!primary&&cn_signal_pending())primary=71;
  auto record_primary=[&](){if(primary&&!exists(s+"/failure.result"))write_new(s+"/failure.result","exit="+std::to_string(primary)+"\nstage="+stage_name+"\nphase="+phase+"\n");};
  try{record_primary();remove_if(s+"/prepared.ready");}catch(const std::exception&){problem=true;}
  try{stop_continuous(s);}catch(const std::exception&e){problem=true;fprintf(stderr,"STACK_CLEANUP_STOP_UNCONFIRMED detail=%s\n",e.what());}
  auto join=[&](ProcessChild&child,const char*name){if(child.pid<0||child.done)return;try{for(unsigned i=0;!child.poll();i++){require(i<999,name,90);delay(200);}}catch(const std::exception&e){problem=true;fprintf(stderr,"STACK_CLEANUP_JOIN step=%s detail=%s\n",name,e.what());}};
  join(capture,"CAPTURE_RPC_ACTIVE");join(utility,"NATIVE_TOOL_ACTIVE");
  // render-probe 未退出时仍可能持有原厂 DMA；不 TERM，也不恢复/开放下一任务。
  bool consumers_gone=utility.pid<0||utility.done;
  if(consumers_gone&&exists(s+"/render.used")){try{render_drained(s);}catch(const std::exception&e){consumers_gone=false;problem=true;fprintf(stderr,"STACK_CLEANUP_RENDER_UNCONFIRMED detail=%s\n",e.what());}}
  if(consumers_gone){try{restore_rc=restore(s);}catch(const std::exception&e){restore_rc=90;fprintf(stderr,"STACK_CLEANUP_RESTORE detail=%s\n",e.what());}}
  if(!restore_rc){try{live_rc=0;if(exists(s+"/live.before")){auto expected=value(s+"/live.before");require(expected=="0"||expected=="1","stack-live-snapshot",90);if(get("camera","live_view_state","int32")!=expected)live(expected=="1");require(get("camera","live_view_state","int32")==expected,"stack-live-restore",90);}}catch(const std::exception&){live_rc=90;}}
  if(restore_rc||live_rc)problem=true;
  if(storage.pid>0&&!storage.done){try{require(storage.poll(),"stack-storage-not-exited",90);}catch(const std::exception&){problem=true;}}
  try{receipt(s,"cleanup.restore.rc",std::to_string(restore_rc));receipt(s,"cleanup.live.rc",std::to_string(live_rc));receipt(s,"cleanup.phase",problem?"RETAINED":"RELEASING_LOCK");sync();}catch(const std::exception&){problem=true;}
  if(!problem){try{lock.release();lock_rc=0;}catch(const std::exception&){lock_rc=90;problem=true;}}
  if(!primary&&cn_signal_pending())primary=71;
  try{record_primary();}catch(const std::exception&){problem=true;}
  int final_rc=problem?90:primary;
  try{write_new(s+"/cleanup.result","primary_rc="+std::to_string(primary)+"\nrestore_rc="+std::to_string(restore_rc)+"\nlive_rc="+std::to_string(live_rc)+"\nlock_rc="+std::to_string(lock_rc)+"\nfinal_rc="+std::to_string(final_rc)+"\n");write_new(s+"/run.rc",std::to_string(final_rc)+"\n");}catch(const std::exception&){final_rc=90;}sync();return final_rc;
 }
 int run(){
  lock.acquire();int primary=0;
  try{
   mkdir_new(s+"/run.lock");phase="DESTINATIONS";require(!exists(s+"/capture.log")&&!exists(s+"/frame-0.3FR"),"stack-existing-capture");fixed_hash(module,module_hash);for(const auto&p:{std::string("/system/bin/camera-storage"),std::string("/system/lib64/libQt6Core.so.6")})require(regular(p,true),"stack-component");
   phase="NATIVE_IDLE";unsigned stable=0;for(unsigned i=0;stable<5;i++){require(i<20,"stack-native-idle-timeout");stable=idle()?stable+1:0;if(stable<5)delay(100);}idle_admission();stage("prepare");
   phase="USER_SETTINGS";save_settings(s,settings(),true);original_storage();readonly(true);armed=true;
   tool(Tool::Inputs,{"plan-output","/mnt/media_rw/ssd/DCIM",s+"/output-plan"},"plan.log");native_result(invoke(cn_counter_main,{"native","counter-sync",s,"before"}),"stack-counter-before");
   auto self=identify(getpid());write_new(s+"/run.start",self.start+"\n");watch.launch(s+"/watchdog.log",[&](){return watchdog(s,self.pid);});
   write_new(s+"/stack.config","RS1 "+std::to_string(cfg.method)+" "+std::to_string(cfg.noise)+"\n");if(cfg.target_iso)write_new(s+"/iso.target",std::to_string(cfg.target_iso)+"\n");
   phase="STORAGE_TAKEOVER";write_new(s+"/storage.stopped","");sync();service("camera-storage",false);for(unsigned i=0;!pids("camera-storage").empty();i++){require(i<49,"stack-storage-stop",23);delay(100);}
   storage.launch({},[&](){return exec_tool(Tool::Storage,{},s);},[&](){write_new(s+"/storage.start",storage.start+"\n");write_new(s+"/storage.pid",std::to_string(storage.pid)+"\n");sync();},true);
   for(unsigned i=0;!exact(s+"/hook.ready","RAW_STACK_PREPARED");i++){alive_local();pump();require(i<19,"stack-hook-timeout",23);delay(1000);}owned_storage(s,storage.pid);require(exact(s+"/abort.supported","RAW_STACK_ABORT_PROTOCOL_V1"),"stack-abort-protocol-required",90);
   phase="STORAGE_READY";idle_admission(true);
   phase="PREPARED";write_new(s+"/prepared.ready","PREPARED\n");auto prepare_end=monotonic_ms()+1800000;while(!exists(s+"/manual.trigger")){alive_local();require(monotonic_ms()<prepare_end,"stack-prepare-timeout",29);delay(100);}require(exact(s+"/manual.trigger","GO"),"stack-manual-trigger",19);remove_regular(s+"/prepared.ready");
   phase="TRIGGER_ADMISSION";idle_admission(true);auto v=settings();save_settings(s,v,false);receipt(s,"format-source",v.format);live_before=get("camera","live_view_state","int32");require(live_before=="0"||live_before=="1","stack-live-before",19);write_new(s+"/live.before",live_before+"\n");if(live_before=="1")live(false);
   for(unsigned i=0;get("camera","stream_group","int32")!="255";i++){require(i<49,"stack-stream-not-idle",19);delay(100);}write_new(s+"/settings.modified","");sync();put("image_format","0");put("drive_mode",cfg.target_iso?"0":"1");
   receipt(s,"stack.progress","0 0 "+std::to_string(cfg.seconds));auto delay_end=monotonic_ms()+cfg.delay*1000;if(cfg.delay){stage("delay");while(monotonic_ms()<delay_end){cancel_before_capture(s);alive_current();poll_hook();auto now=monotonic_ms();receipt(s,"countdown",std::to_string(now<delay_end?(delay_end-now+999)/1000:0));delay(100);}receipt(s,"countdown","0");}
   cancel_before_capture(s);deadline=monotonic_ms()+uint64_t(cfg.seconds)*1000;uint64_t previous_start=0,previous_ready=0;stage("capture");phase="CAPTURE";
   if(!cfg.target_iso)continuous();
   while(cfg.target_iso&&continue_frames(count,monotonic_ms(),deadline,exists(s+"/finish.request"),cfg,iso)){
    local_cancel();auto control_start=monotonic_ms();next_error=0;alive_current();require(get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0","stack-exposure-admission",19);
    auto next=count+1;receipt(s,"frame.request","RS1 "+std::to_string(next));wait_token("frame.armed","RS1 "+std::to_string(next));auto start=monotonic_ms();if(previous_start)gap=start-previous_start;previous_start=start;
    if(!exists(s+"/capture.invoked"))write_new(s+"/capture.invoked","");remove_if(s+"/capture.rc");require(capture.pid<0||capture.done,"stack-rpc-overlap",90);capture=ProcessChild{};
    capture.launch({},[&](){Recovery detached;auto result=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=180000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.do_exposure_extended2","boolean:true","int32:12","int32:180000"},180000,false);receipt(s,"capture.rc",std::to_string(result.rc));write_replace(s+"/capture-request.log",result.output);sync();return result.rc;},[&](){receipt(s,"capture.rpc.start",capture.start);receipt(s,"capture.rpc.pid",std::to_string(capture.pid));sync();},true);
    auto wait_end=wait_token("frame.accepted","RS1 "+std::to_string(next));while(!exists(s+"/capture.rc")||get("camera","exposure_status","int32")!="0"){alive_current();capture_result();poll_hook();require(monotonic_ms()<wait_end,"stack-exposure-idle-timeout",26);progress();delay(10);}native_result(capture.wait(),"stack-capture-rpc");capture_result();alive_current();auto accepted=monotonic_ms();
    printf("RAW_STACK_FRAME_HANDOFF sequence=%u request_ms=%llu ready_ms=%llu handoff_ms=%llu\n",next,(unsigned long long)start,(unsigned long long)accepted,(unsigned long long)(accepted-start));printf("RAW_STACK_FRAME_CONTROL sequence=%u before_request_ms=%llu previous_ready_to_request_ms=%llu\n",next,(unsigned long long)(start-control_start),(unsigned long long)(previous_ready?start-previous_ready:0));previous_ready=accepted;count=next;progress();
   }
   require(count>0,"stack-no-frame-no-output",28);next_error=0;alive_current();receipt(s,"finish.request","FINISH");stage("merge");phase="FINAL_RAW";wait_token("raw.ready","RAW_STACK_R1");progress();require(natural_drained(s),"stack-finish-drain",90);puts("RAW_STACK_FINISHED_ALL_FRAMES_RELEASED");
   stage("restore");native_result(restore(s),"stack-storage-restore");stage("render");phase="RENDER";require(value("/tmp/x2d-pixel-render-route.status")=="READY","stack-render-route",30);live(false);for(unsigned i=0;get("camera","stream_group","int32")!="255";i++){require(i<49,"stack-render-stream",30);delay(100);}require(get("camera","live_view_state","int32")=="0","stack-render-live",30);puts("RAW_STACK_RENDER_STREAM_IDLE_AFTER_STORAGE_RESTORE");
   write_new(s+"/render.used","");tool(Tool::Render,{"--raw-stack",s},"probe.log");render_drained(s);require(get("error","active_error","boolean")=="false"&&log_count(s+"/probe.log","RAW_STACK_NATIVE_JPEG_AND_FINAL_PREVIEW_COMPLETE")==1,"stack-render-complete",30);
   stage("raw");phase="FINALIZE";std::vector<std::string> final_args={s};if(cfg.target_iso)final_args.push_back("--iso");tool(Tool::Finalize,final_args,"finalize.log");tool(Tool::Pack,{s+"/render.jpg",s+"/preview.jpg",s+"/album.jpg","--direct-staged-output","--capture-raw",s+"/merged.3FR"},"pack.log");
   if(v.format=="3"||v.format=="4"){stage("convert");phase="HEIF";auto info=read("/proc/meminfo",16384);uint64_t available=0;require(cn_mem_available(info.data(),info.size(),&available)&&available>=262144,"stack-heif-memory",19);tool(Tool::Heif,{s},"heif.log");require(exact(s+"/heif.ready","HEIF_ENCODED 11656 8742 6"),"stack-heif-receipt");}
   stage("publish");phase="PUBLISH";bind_raw_publication(s);native_result(invoke(cn_publish_main,{"native","publish-stack",s,v.format}),"stack-publish");native_result(invoke(cn_counter_main,{"native","counter-sync",s,"after"}),"stack-counter-after");require(regular(s+"/album-location"),"stack-album-location");
   for(const auto*name:{"rawpb.bin","preview.jpg","album.jpg","frame.request","frame.armed","frame.done","finish.request"})remove_if(s+"/"+name);puts("RAW_STACK_FINAL_RAW_AND_SELECTED_COMPRESSED_IMAGE_COMPLETE");
  }catch(const Error&e){primary=e.rc==143?71:e.rc;recovery_problem=e.recovery_failed;fprintf(stderr,"STACK_RUN_PRIMARY stage=%s phase=%s error=%s rc=%d errno=%d\n",stage_name.c_str(),phase.c_str(),e.what(),primary,e.system_errno);}catch(const std::exception&e){primary=1;fprintf(stderr,"STACK_RUN_PRIMARY error=%s\n",e.what());}
  if(armed)return cleanup(primary);Recovery recovery;try{lock.release();}catch(const std::exception&){return 90;}return primary;
 }
};
} }
extern "C" int cn_stack_runner_main(int argc,char**argv){using namespace cn;using namespace cn::stack_runner;try{require(argc>=3,"stack-arguments",2);std::string mode=argv[1],s=argv[2];job(s);if(mode=="stack-run"){require(argc==8&&std::string(argv[7])=="--prepare","stack-run-arguments",2);Runner runner(s,config(argv[3],argv[4],argv[5],argv[6]));return runner.run();}if(mode=="stack-restore"){require(argc==3,"stack-restore-arguments",2);return restore(s);}if(mode=="stack-watchdog"){require(argc==4,"stack-watchdog-arguments",2);return watchdog(s,parse_pid(argv[3]));}throw Error("stack-command",2);}catch(const Error&e){fprintf(stderr,"STACK_RUN_REFUSED step=%s rc=%d\n",e.what(),e.rc);return e.recovery_failed?90:e.rc==143?71:e.rc;}catch(const std::exception&e){fprintf(stderr,"STACK_RUN_REFUSED detail=%s\n",e.what());return 1;}}
