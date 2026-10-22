/* 4.2.0 原生启动适配；不复制 run/render/publish/convert 脚本到任务目录。 */
#include "native_common.hpp"
#include "native_runtime_tables.hpp"
#include "native_memory_runner.hpp"
#include "native_disk_runner.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <sstream>
extern char **environ;
namespace cn { namespace launch {
struct FD{int n;explicit FD(int v):n(v){require(n>=0,"launch-directory-open");}~FD(){close(n);}FD(const FD&)=delete;};
static int directory_path(const std::string&path){require(!path.empty()&&path.front()=='/',"launch-path",2);int parent=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);require(parent>=0,"launch-root");try{std::istringstream pieces(path.substr(1));std::string name;while(std::getline(pieces,name,'/')){require(!name.empty()&&name!="."&&name!="..","launch-component",2);int next=openat(parent,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);require(next>=0,"launch-parent");close(parent);parent=next;}return parent;}catch(...){close(parent);throw;}}
static void file(const std::string&path){struct stat st;require(!lstat(path.c_str(),&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"launch-file:"+path);}
static void fixed(const std::string&path){const char*digest=nullptr;for(const auto&t:runtime::guards)if(path==t.path){require(digest==nullptr,"launch-duplicate-manifest");digest=t.digest;}
    require(digest!=nullptr,"launch-component-not-in-manifest");file(path);require(sha256(path)==digest,"launch-component-digest:"+path);}
static void clear_endpoint(){
    FD dir(directory_path("/tmp/x2d-pixel-overclock"));struct stat owner;if(fstatat(dir.n,"memory.sock",&owner,AT_SYMLINK_NOFOLLOW)){require(errno==ENOENT,"launch-endpoint-state");return;}
    require(S_ISSOCK(owner.st_mode),"launch-endpoint-not-socket",73);FD tmp(directory_path("/tmp"));
    require(!mkdirat(tmp.n,"x2d-multishot-active",0700),"launch-endpoint-active-task",72);struct stat lock{};bool known=!fstatat(tmp.n,"x2d-multishot-active",&lock,AT_SYMLINK_NOFOLLOW);int primary=0,primary_errno=0;std::string reason;
    try{require(known&&S_ISDIR(lock.st_mode),"launch-endpoint-lock",90);std::istringstream lines(read("/proc/net/unix",4*1024*1024));std::string line;require(bool(std::getline(lines,line))&&line.rfind("Num",0)==0,"launch-unix-table-header",74);
        while(std::getline(lines,line)){std::istringstream row(line);std::string column;for(unsigned i=0;i<7;i++)require(bool(row>>column),"launch-unix-table-record",74);std::string path;std::getline(row,path);auto start=path.find_first_not_of(' ');if(start!=std::string::npos)require(path.substr(start)!="/tmp/x2d-pixel-overclock/memory.sock","launch-endpoint-listening",75);}
        require(pids("render-probe").empty()&&pids("render-memory").empty(),"launch-render-process-active",76);struct stat now;require(!fstatat(dir.n,"memory.sock",&now,AT_SYMLINK_NOFOLLOW)&&S_ISSOCK(now.st_mode)&&now.st_dev==owner.st_dev&&now.st_ino==owner.st_ino,"launch-endpoint-changed",90);require(!unlinkat(dir.n,"memory.sock",0),"launch-endpoint-remove");require(!fsync(dir.n),"launch-endpoint-sync");puts("MEMORY_STALE_ENDPOINT_REMOVED_BEFORE_PREPARATION");
    }catch(const Error&e){primary=e.rc;primary_errno=e.system_errno;reason=e.what();}catch(const std::exception&e){primary=1;reason=e.what();}
    try{Recovery recovery;struct stat now;require(known&&!fstatat(tmp.n,"x2d-multishot-active",&now,AT_SYMLINK_NOFOLLOW)&&now.st_dev==lock.st_dev&&now.st_ino==lock.st_ino,"launch-endpoint-lock-changed",90);if(unlinkat(tmp.n,"x2d-multishot-active",AT_REMOVEDIR))throw Error("launch-endpoint-unlock",90,true,errno);}
    catch(const Error&e){fprintf(stderr,"NATIVE_LAUNCH_ENDPOINT_RECOVERY primary_rc=%d primary_step=%s primary_errno=%d restore_rc=90 restore_step=%s restore_errno=%d\n",primary,reason.c_str(),primary_errno,e.what(),e.system_errno);throw Error(primary?reason:e.what(),primary?primary:90,true,primary?primary_errno:e.system_errno);}
    if(primary)throw Error(reason,primary,false,primary_errno);
}
static void ownership(const std::string&job){const auto name=job.substr(job.find_last_of('/')+1);write_new(job+"/native-owner","X2D_NATIVE_TASK_V1\njob="+name+"\n");}
static int start(std::vector<std::string> args){
    cancelled();const char*program="/system/bin/x2d-native-flow";struct stat installed,running;require(!lstat(program,&installed)&&S_ISREG(installed.st_mode)&&installed.st_nlink==1&&!stat("/proc/self/exe",&running)&&installed.st_dev==running.st_dev&&installed.st_ino==running.st_ino,"launch-native-executable-identity");
    // exec 更新真实 cmdline，使 watchdog 对入口与本次任务的核对仍有效。
    args[0]=program;std::vector<char*>argv;for(auto&s:args)argv.push_back(&s[0]);argv.push_back(nullptr);execve(program,argv.data(),environ);throw Error("launch-native-exec",127,false,errno);
}
static int run(int argc,char**argv){
    if(argc>=2&&std::string(argv[1])=="launch-stack"){
        require(argc==8&&std::string(argv[7])=="--prepare","launch-stack-arguments",2);
        std::string task_path=argv[2];job(task_path);FD task(directory_path(task_path));
        for(const char*n:{"run.sh","native-owner","run.lock","run.rc","capture.invoked"})require(!exists(task_path+"/"+n),"launch-reused-job",3);
        // 方法、时间/ISO、降噪与延时仍由真正 stack-run 入口严格验证；这里不把任意文本交给 shell。
        for(int i=3;i<7;++i)require(digits(argv[i])&&std::string(argv[i]).size()<=10,"launch-stack-number",2);
        file("/system/bin/x2d-raw-stack/libx2d_raw_stack.so");
        fixed("/system/bin/x2d-raw-stack/stack-pack");fixed("/system/bin/x2d-raw-stack/finalize");fixed("/system/bin/x2d-pixel-overclock/render-probe");
        ownership(task_path);return start({argv[0],"stack-run",task_path,argv[3],argv[4],argv[5],argv[6],"--prepare"});
    }
    require(argc==6&&std::string(argv[5])=="--prepare","launch-arguments",2);std::string command=argv[1],job_path=argv[2],delay=argv[3],frames=argv[4];require(command=="launch-memory"||command=="launch-multi","launch-command",2);
    require(digits(delay)&&delay.size()<=2&&(delay=="0"||delay.front()!='0'),"launch-delay",2);unsigned d=unsigned(std::stoul(delay));
    if(command=="launch-memory")require(frames=="6"&&d>=2&&d<=60,"launch-memory-mode",2);else require((frames=="25"&&d==0)||(frames=="16"&&d>=2&&d<=60),"launch-multi-mode",2);
    job(job_path);FD task(directory_path(job_path));for(const auto*n:{"run.sh","memory.version","native-owner","run.lock","run.rc","runner.final","capture.invoked"})require(!exists(job_path+"/"+n),"launch-reused-job",3);
    if(command=="launch-memory"){
        for(const char*n:{"libx2d_private_memory_capture.so","render-memory","fix-3fr","pack","preview","inputs","heif"})fixed(std::string("/system/bin/x2d-pixel-overclock/")+n);
        for(const char*p:{"/system/etc/imx461bqr_ec1706.conf","/system/lib64/camera/plugins/lib_frog_e2_ec1706_native.so","/system/bin/camera-gui"})file(p);
        file("/tmp/x2d-pixel-render-route.status");require(value("/tmp/x2d-pixel-render-route.status")=="READY","launch-render-route");clear_endpoint();auto format=get("camera","image_format","int32");require(format=="0"||format=="1"||format=="2","COMPAT_MEMORY_TEST_REQUIRES_RAW_OR_JPEG",2);write_new(job_path+"/format-source",format+"\n");write_new(job_path+"/memory.version","MEMORY_MANUAL_R1\n");ownership(job_path);return start({argv[0],"memory-run",job_path,delay,"6","--prepare"});
    }
    for(const char*n:{"libquarter_capture.so","finalize25","render25","pack25","merge16","render16","jpeg-grid","pack16","libx2d_private_capture.so"})fixed(std::string("/system/bin/x2d-multi-resolution/")+n);
    if(frames=="16")clear_endpoint();ownership(job_path);if(frames=="25")return start({argv[0],"quarter-run",job_path,"0","86399","0","0","--prepare"});return start({argv[0],"sixteen-run",job_path,delay,"16","--prepare"});
}
} }
extern "C" int cn_launch_main(int argc,char**argv){try{return cn::launch::run(argc,argv);}catch(const cn::Error&e){fprintf(stderr,"NATIVE_LAUNCH_REFUSED step=%s primary_rc=%d restore_rc=%d errno=%d\n",e.what(),e.rc,e.recovery_failed?90:0,e.system_errno);return e.recovery_failed?90:e.rc;}catch(const std::exception&e){fprintf(stderr,"NATIVE_LAUNCH_REFUSED detail=%s\n",e.what());return 1;}}
