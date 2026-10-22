/* HEIF 独立转换/恢复。原始转换失败与菜单/取景恢复失败分别记录。 */
#include "native_process.hpp"
#include "native_ui_contract.hpp"
#if CN_NATIVE_UI_ENABLED
#include "native_ui_ready.hpp"
#endif
extern "C" {
#include "camera_native.h"
}
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <sstream>
namespace cn {
#if !CN_NATIVE_UI_ENABLED
static const std::string menu_status="/tmp/x2d-native-menu-preload.status";
#endif
static void idle(){require(get("camera","exposure_status","int32")=="0"&&get("error","active_error","boolean")=="false","heif-native-idle");}
static void live(bool on){auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=3000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.set_live_view",on?"boolean:true":"boolean:false"});require(!r.rc,"heif-live",r.rc);}
static bool all_running(){return !pids("camera-service").empty()&&!pids("camera-storage").empty()&&!pids("camera-gui").empty();}
static bool all_stopped(){return pids("camera-service").empty()&&pids("camera-storage").empty()&&pids("camera-gui").empty();}
#if !CN_NATIVE_UI_ENABLED
static std::string gui_identity(){
    auto list=pids("camera-gui");require(list.size()==1&&executable(list[0])=="/system/bin/camera-gui","heif-gui-identity");
    auto stat=read("/proc/"+std::to_string(list[0])+"/stat",16384);auto end=stat.rfind(')');require(end!=std::string::npos,"heif-gui-stat");
    std::istringstream fields(stat.substr(end+1));std::string start;for(unsigned i=3;i<=22;i++)require(bool(fields>>start),"heif-gui-start");
    require(digits(start),"heif-gui-start-value");return std::to_string(list[0])+":"+start;
}
static void menu_restore(const std::string&s){
    require(value(menu_status)=="SKIPPED_RETRY","heif-menu-retry-state");
    // H3 v4 的实际 p18 摘要；菜单库更新时构建契约必须同步更新。
    require(sha256("/system/lib64/libx2d_native_menu.so")=="70a1ea682992a7f7d8d3e3ae712fafe25af90e1446826b82cd0bd44bcdb0053e","heif-menu-library-hash");
    const std::string attempt="/tmp/x2d-native-menu-attempt",backup=attempt+".before-"+s.substr(s.find_last_of('/')+1)+"-heif-restored";
    require(regular(attempt)&&read(attempt,1).empty()&&!exists(backup),"heif-menu-attempt");idle();bool started=false;
    try{
        service("camera-gui",false);unsigned i=0;while(!pids("camera-gui").empty()){require(++i<50,"heif-gui-stop-timeout");sleep_ms(100);}
        require(read(attempt,1).empty(),"heif-menu-attempt-changed");move_new(attempt,backup,false);
        service("camera-gui",true);started=true;i=0;
        for(;;){bool ready=false;try{ready=value(menu_status)=="MENU_AND_AFC_POINTERS_READY";}catch(const Error&){}if(ready)break;require(++i<15,"heif-menu-ready-timeout");sleep_ms(1000);}
        auto expected_gui=gui_identity();sleep_ms(2000);require(gui_identity()==expected_gui,"heif-gui-changed-during-stability");require(value(menu_status)=="MENU_AND_AFC_POINTERS_READY","heif-menu-unstable");idle();
        puts("EXISTING_ENHANCED_MENU_RELOADED");
    }catch(const std::exception&e){
        fprintf(stderr,"HEIF_MENU_PRIMARY detail=%s\n",e.what());
        if(!started){try{service("camera-gui",true);}catch(const std::exception&r){fprintf(stderr,"HEIF_GUI_RESTART_UNCONFIRMED detail=%s\n",r.what());}}
        throw;
    }
}
#endif
static void menu_ready(){
#if CN_NATIVE_UI_ENABLED
    native_ui_ready({CN_NATIVE_UI_SHA256,CN_NATIVE_GUI_SHA256,CN_NATIVE_UI_CAPABILITIES});
#else
    require(value(menu_status)=="MENU_AND_AFC_POINTERS_READY","heif-menu-not-ready");
#endif
}
static int heif_main(int argc,char**argv){
    bool stopped=false,live_changed=false;std::string previous,s;int primary=0,restore=0;Child encoder(false);
    try{
        require(argc==3,"heif-arguments",2);s=argv[2];job(s);cancelled();
        for(const auto*n:{"heif.restore","heif.pid","album.heic","heif.ready","heif.log"})require(!exists(s+"/"+n),"heif-existing-output");
        require(regular(s+"/album.jpg",true),"heif-jpeg-input");menu_ready();idle();
        previous=get("camera","live_view_state","int32");require(previous=="0"||previous=="1","heif-live-state",2);
        if(previous=="1"){live_changed=true;live(false);}
        stopped=true;service("camera-gui",false);service("camera-storage",false);service("camera-service",false);
        unsigned i=0;while(!all_stopped()){require(++i<50,"heif-services-stop-timeout");sleep_ms(100);}
        auto memory=read("/proc/meminfo",16384);uint64_t available=0;require(cn_mem_available(memory.data(),memory.size(),&available)&&available>=573440,"heif-memory-insufficient");
        const std::string tool="/system/bin/x2d-pixel-overclock/heif";
        encoder.start({tool,s},s+"/heif.log");write_new(s+"/heif.pid",std::to_string(encoder.pid())+"\n");i=0;
        while(!encoder.poll()){require(++i<900,"heif-encoder-timeout");sleep_ms(1000);}
        int rc=encoder.wait();cancelled();require(!rc,"heif-encoder",rc);
        require(read(s+"/heif.log",16*1024*1024).find("x265 [error]")==std::string::npos,"heif-encoder-log-error");
        require(value(s+"/heif.ready")=="HEIF_ENCODED 23326 17498 15","heif-ready");
        struct stat output;require(!lstat((s+"/album.heic").c_str(),&output)&&S_ISREG(output.st_mode)&&output.st_nlink==1&&output.st_size>0&&output.st_size%4096==0,"heif-output-missing-or-unaligned");puts("HEIF_ENCODED_DIMENSIONS_VERIFIED");
    }catch(const Error&e){primary=e.rc;if(e.recovery_failed)restore=90;fprintf(stderr,"HEIF_PRIMARY step=%s rc=%d\n",e.what(),primary);}
    catch(const std::exception&e){primary=1;fprintf(stderr,"HEIF_PRIMARY detail=%s rc=1\n",e.what());}
    {
        Recovery recovery;
        if(encoder.pid()>0){try{encoder.terminate_heif_bounded();}catch(const std::exception&e){restore=90;fprintf(stderr,"HEIF_ENCODER_EXIT_UNCONFIRMED detail=%s\n",e.what());}}
        // 未确认本次 encoder 退出，不擅自并发重启原厂服务争抢内存。
        if(stopped&&(encoder.pid()<=0||encoder.done())){
            for(const auto*name:{"camera-service","camera-storage","camera-gui"}){try{service(name,true);}catch(const std::exception&e){restore=90;fprintf(stderr,"HEIF_SERVICE_RESTORE name=%s detail=%s\n",name,e.what());}}
            try{
                unsigned i=0;while(!all_running()){require(++i<30,"heif-services-start-timeout");sleep_ms(1000);}i=0;
                for(;;){bool ready=false;try{idle();ready=true;}catch(const Error&){}if(ready)break;require(++i<30,"heif-services-idle-timeout");sleep_ms(1000);}i=0;
#if CN_NATIVE_UI_ENABLED
                menu_ready();
                puts("NATIVE_UI_CURRENT_REQUEST_RESTORE_CONFIRMED");
#else
                for(;;){bool ready=false;try{ready=value(menu_status)=="SKIPPED_RETRY";}catch(const Error&){}if(ready)break;require(++i<15,"heif-menu-retry-timeout");sleep_ms(1000);}
                menu_restore(s);
#endif
            }catch(const std::exception&e){restore=90;fprintf(stderr,"HEIF_MENU_RESTORE_UNCONFIRMED detail=%s\n",e.what());}
        }
        // 菜单失败仍尝试恢复已经修改的取景状态，保留独立失败。
        if((stopped||live_changed)&&(previous=="0"||previous=="1")&&(encoder.done()||encoder.pid()<=0)){
            try{if(get("camera","live_view_state","int32")!=previous)live(previous=="1");require(get("camera","live_view_state","int32")==previous,"heif-live-verify");}
            catch(const std::exception&e){restore=90;fprintf(stderr,"HEIF_LIVE_RESTORE_UNCONFIRMED detail=%s\n",e.what());}
        }
        if(stopped&&!restore){try{idle();write_new(s+"/heif.restore","RESTORED\n");}catch(const std::exception&e){restore=90;fprintf(stderr,"HEIF_RESTORE_RECEIPT_UNCONFIRMED detail=%s\n",e.what());}}
    }
    if(!primary&&stopping())primary=143;
    fprintf(stderr,"HEIF_FINISH primary_rc=%d restore_rc=%d\n",primary,restore);return restore?restore:primary;
}
}
extern "C" int cn_heif_restart_main(int argc,char**argv){return cn::heif_main(argc,argv);}
