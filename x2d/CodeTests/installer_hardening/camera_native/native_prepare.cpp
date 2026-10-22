/* 固定预热缓存协议。准备只预读目录与状态，不改变相机设置。 */
#include "native_process.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sstream>
#include <cstdio>
namespace cn {
static const std::string cache="/tmp/x2d-pixel-overclock/memory-prepared";
static const std::string arm="/tmp/x2d-pixel-overclock/arm.txt";
static const std::string active="/tmp/x2d-multishot-active";
static uint64_t uptime(){auto t=read("/proc/uptime",4096);auto n=t.substr(0,t.find('.'));require(digits(n)&&n.size()<=12,"prepare-uptime",3);return std::stoull(n);}
static std::string identity(){
    std::string id;
    for(const auto*name:{"camera-service","camera-storage"}){
        auto found=pids(name);require(found.size()==1&&executable(found[0])==std::string("/system/bin/")+name,"prepare-service-identity");
        auto stat=read("/proc/"+std::to_string(found[0])+"/stat",16384);auto end=stat.rfind(')');require(end!=std::string::npos,"prepare-proc-stat");
        // comm 可含空格；结束括号之后从 field3 计数到 starttime(field22)。
        std::istringstream fields(stat.substr(end+1));std::string start;
        for(unsigned i=3;i<=22;i++)require(bool(fields>>start),"prepare-proc-starttime");
        require(digits(start),"prepare-starttime-type");id+=std::to_string(found[0])+":"+start+" ";
    }
    return id;
}
static std::string arm_identity(){struct stat st;require(!lstat(arm.c_str(),&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"prepare-arm-type");return std::to_string(st.st_ino)+":"+std::to_string(st.st_size)+":"+std::to_string(st.st_mtim.tv_sec);}
static bool idle(){try{return get("camera","exposure_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("storage","storage_status","int32")=="0";}catch(const Error&e){if(e.rc==143)throw;return false;}}
static void owned(){
    require(directory(cache),"prepare-cache-directory");DIR*d=opendir(cache.c_str());require(d!=nullptr,"prepare-cache-open");
    try{while(auto*e=readdir(d)){std::string n=e->d_name;if(n=="."||n=="..")continue;require(n=="ready"||n=="ready.tmp"||n=="plan"||n=="plan.log","prepare-cache-unowned-entry");require(regular(cache+"/"+n),"prepare-cache-file-type");}}
    catch(...){closedir(d);throw;}closedir(d);
}
static void clear_files(){owned();for(const auto*n:{"ready","ready.tmp","plan","plan.log"})if(exists(cache+"/"+n))remove_regular(cache+"/"+n);}
static bool current(const std::string&id,const std::string&armid){try{return identity()==id&&arm_identity()==armid&&!exists(active);}catch(const Error&e){if(e.rc==143)throw;return false;}}
static int prepare_main(int argc,char**argv){
    bool running=false;int result=0;
    try{
        require(argc==3,"prepare-arguments",2);std::string mode=argv[2];cancelled();
        if(mode=="--clear"){if(exists(cache)){clear_files();require(!rmdir(cache.c_str()),"prepare-cache-remove");}return 0;}
        if(mode=="--check"){
            owned();auto text=value(cache+"/ready",1024);std::istringstream fields(text);std::string token,stamp,camera,storage,armid,extra;
            require(bool(fields>>token>>stamp>>camera>>storage>>armid)&&!(fields>>extra)&&token=="READY","prepare-cache-record");
            require(digits(stamp)&&stamp.size()<=12,"prepare-cache-stamp",3);uint64_t now=uptime(),then=std::stoull(stamp);require(now>=then&&now-then<=3,"prepare-cache-stale",3);
            require(camera+" "+storage+" "==identity()&&armid==arm_identity()&&idle(),"prepare-cache-changed");puts("MEMORY_MODE_PREPARED_CACHE_CURRENT");return 0;
        }
        require(mode=="--prepare","prepare-mode",2);require(!exists(active),"prepare-task-active");require(directory("/tmp/x2d-pixel-overclock"),"prepare-root");
        if(exists(cache))clear_files();else mkdir_new(cache);
        // 当前入口使用当前库，取消旧 r17 库仅为预热而被强制打包的依赖。
        for(const auto*p:{"/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so","/system/bin/camera-storage","/system/bin/camera-service","/system/lib64/libQt6Core.so.6"})require(regular(p)&&!access(p,R_OK),"prepare-input");
        auto id=identity(),armid=arm_identity();Child planner;planner.start({"/system/bin/x2d-pixel-overclock/inputs","plan-output","/mnt/media_rw/ssd/DCIM",cache+"/plan"},cache+"/plan.log");
        int rc=planner.wait_utility();cancelled();require(!rc,"prepare-plan",rc);running=true;unsigned stable=0;
        while(current(id,armid)){
            if(idle()){if(stable<5)stable++;}else{stable=0;if(exists(cache+"/ready"))remove_regular(cache+"/ready");}
            if(stable>=5){write_new(cache+"/ready.tmp","READY "+std::to_string(uptime())+" "+id+armid+"\n");
                if(exists(cache+"/ready"))require(regular(cache+"/ready"),"prepare-ready-type");require(!rename((cache+"/ready.tmp").c_str(),(cache+"/ready").c_str()),"prepare-ready-commit");}
            sleep_ms(stable<5?100:1000);
        }
    }catch(const Error&e){result=e.recovery_failed?90:e.rc;fprintf(stderr,"PREPARE_FAILURE step=%s primary_rc=%d restore_rc=%d\n",e.what(),e.rc,e.recovery_failed?90:0);}
    catch(const std::exception&e){result=1;fprintf(stderr,"PREPARE_FAILURE detail=%s rc=1\n",e.what());}
    if(running){Recovery restore;try{owned();if(exists(cache+"/ready"))remove_regular(cache+"/ready");}catch(const std::exception&e){fprintf(stderr,"PREPARE_CLEANUP_UNCONFIRMED detail=%s primary_rc=%d\n",e.what(),result);result=90;}}
    return result;
}
}
extern "C" int cn_prepare_main(int argc,char**argv){return cn::prepare_main(argc,argv);}
