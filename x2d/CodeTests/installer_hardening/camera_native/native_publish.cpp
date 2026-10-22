/* 各发布路径保持独立模式，成片提交与相册登记分别确认。 */
#include "native_common.hpp"
#include <unistd.h>
#include <cstdio>
#include <cstring>
extern "C" void cn_recovery_mode(int);
namespace cn {
static void live(bool on){auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.set_live_view",on?"boolean:true":"boolean:false"});require(!r.rc,"set-live",r.rc);}
static bool idle(){return get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("camera","live_view_state","int32")=="0"&&get("camera","stream_group","int32")=="255";}
static bool browse(const std::string&job,const Album&p){
    auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=5000","--dest=com.hasselblad.storage","/storage","com.hasselblad.storage.browse","string:/ssd/"+p.folder,"int32:0"});
    write_replace(job+"/album-browse.log",r.output);require(!r.rc,"album-browse-rpc",31);
    return r.output.find("string \""+p.image+"\"")!=std::string::npos;
}
static pid_t one_pid(const std::string&name,int rc){auto p=pids(name);require(p.size()==1,"single-service-pid",rc);require(executable(p[0])=="/system/bin/"+name,"service-executable",rc);return p[0];}
static void wait_storage_idle(){
    const auto end=monotonic_ms()+20000;
    for(;;){bool ready=false;try{ready=idle();}catch(const Error&e){
        if(e.rc==143||e.rc==71||std::string(e.what())!="dbus-get:storage.storage_status")throw;
    }if(ready)return;require(monotonic_ms()<end,"refresh-ready-timeout");sleep_ms(250);}
}
static void index_album(const std::string&job,const Album&p){
    if(browse(job,p))return;
    require(!exists(job+"/album-refresh.started"),"refresh-already-attempted");require(idle(),"refresh-idle");
    require(value("/tmp/x2d-pixel-overclock/quarter.txt")=="DISARMED","quarter-armed");
    require(sha256("/system/bin/camera-storage")=="8c086e9c2a340c2a388e92ae3de39d6d0e5ce19ac3d582626112a24fd3b97aef","storage-baseline");
    require(sha256("/system/etc/init/camera-storage.rc")=="d9fb4f501a214b0b455b0385e9a0f43bf9b087660bbfc4141ca8e04e0451a9e0","storage-init-baseline");
    auto old=one_pid("camera-storage",41);auto env=read("/proc/"+std::to_string(old)+"/environ");std::vector<std::string> loader;
    for(size_t pos=0;pos<env.size();){auto end=env.find('\0',pos);require(end!=std::string::npos,"environment-record",42);auto item=env.substr(pos,end-pos);if(item.rfind("LD_",0)==0)loader.push_back(item);pos=end+1;}
    if(!loader.empty()){require(loader.size()==1&&loader[0]=="LD_PRELOAD=/system/lib64/libx2d_quarter_native.so","unexpected-storage-loader",42);require(value("/tmp/x2d-quarter-native.status")=="QUARTER_NATIVE_READY","quarter-not-ready",42);}
    for(const auto&n:{"render-probe","render-memory","render25","render16"})require(pids(n).empty(),"render-consumer-active",43);
    require(!mapped_library({"libx2d_private_memory_capture.so","libx2d_raw_stack.so"}),"capture-consumer-active",43);
    auto camera=one_pid("camera-service",44);copy_new(job+"/album-browse.log",job+"/album-browse.before-refresh.log");
    write_new(job+"/album-refresh.started","old_storage_pid="+std::to_string(old)+"\n");
    bool stopped=false,started=false;int primary=0;
    try{
        require(idle()&&one_pid("camera-storage",41)==old,"refresh-pre-stop");stopped=true;service("camera-storage",false);
        unsigned i=0;while(!pids("camera-storage").empty()){require(++i<=50,"refresh-stop-timeout");sleep_ms(100);}
        service("camera-storage",true);started=true;i=0;
        // init 启动返回不代表 D-Bus 名称已注册；只在此有界启动窗口容忍查询尚不可用。
        wait_storage_idle();
        require(one_pid("camera-storage",45)!=old,"storage-pid-unchanged",45);require(one_pid("camera-service",44)==camera,"camera-service-changed");
        require(browse(job,p),"album-missing-after-refresh",2);puts("ALBUM_ITEM_VERIFIED_AFTER_NATIVE_STORAGE_RESCAN");
    }catch(const Error&e){primary=e.rc==143?71:e.rc;fprintf(stderr,"ALBUM_REFRESH_PRIMARY step=%s rc=%d\n",e.what(),primary);}
    catch(const std::exception&e){primary=1;fprintf(stderr,"ALBUM_REFRESH_PRIMARY detail=%s rc=1\n",e.what());}
    Recovery recovery_scope;int final=primary;
    if(stopped&&!started){try{service("camera-storage",true);}catch(const std::exception&){final=90;}}
    try{write_new(job+"/album-refresh.result","exit="+std::to_string(final)+"\nstarted="+(started?"1\n":"0\n"));}catch(const std::exception&){final=90;}
    require(!final,"album-refresh",final);
}
static void memory_index(const std::string&job,const Album&p){
    if(browse(job,p))return;
    require(get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0"&&get("error","active_error","boolean")=="false","memory-album-idle");
    auto previous=get("camera","live_view_state","int32");require(previous=="0"||previous=="1","live-state",51);
    require(!exists(job+"/album-live.result"),"live-result-exists");int primary=0;
    try{if(previous=="1")live(false);unsigned i=0;while(!idle()){require(++i<=20,"memory-album-idle-timeout");sleep_ms(250);}index_album(job,p);}
    catch(const Error&e){primary=e.rc==143?71:e.rc;fprintf(stderr,"ALBUM_LIVE_PRIMARY step=%s rc=%d\n",e.what(),primary);}
    catch(const std::exception&e){primary=1;fprintf(stderr,"ALBUM_LIVE_PRIMARY detail=%s rc=1\n",e.what());}
    Recovery recovery_scope;int restore=0;
    try{if(previous=="1")live(true);}catch(const std::exception&){restore=90;}
    try{require(get("camera","live_view_state","int32")==previous,"restore-live-state");}catch(const std::exception&){restore=90;}
    try{write_new(job+"/album-live.result","primary_rc="+std::to_string(primary)+"\nlive_rc="+std::to_string(restore)+"\n");}catch(const std::exception&){restore=90;}
    require(!primary&&!restore,"memory-album-index",restore?restore:primary);
}
struct Movement {std::string current,source;bool active=false;};
struct Copy {std::string current;bool active=false;};
int publish_main(int argc,char **argv){
    std::vector<Movement> moved;std::vector<Copy> copied;bool committed=false,cleanup_unconfirmed=false;std::string jobpath,step="arguments";int primary=0;
    try{
        cancelled();moved.reserve(3);copied.reserve(1);
        require(argc==4,"arguments",2);std::string mode=argv[1];jobpath=argv[2];job(jobpath);std::string format=argv[3];
        require(format.size()==1&&format[0]>='0'&&format[0]<='4',"format",2);
        bool stack=mode=="publish-stack",quarter=mode=="publish-quarter",memory=mode=="publish-memory",disk=mode=="publish-disk",legacy=mode=="publish-legacy-memory";
        require(stack||quarter||memory||disk||legacy,"publish-mode",2);
        bool heif=format=="3"||format=="4",raw=stack||quarter||format=="0"||format=="1"||format=="3";
        std::string rawsource=stack||quarter?"merged.3FR":"final.3FR";
        step="inputs";require(regular(jobpath+"/restored"),"restored-marker");
        if(stack||quarter)require(regular(jobpath+"/raw.ready"),"raw-ready-marker");
        if(raw)require(regular(jobpath+"/"+rawsource,true),"raw-input");
        if(!(stack||quarter)||!heif)require(regular(jobpath+"/album.jpg",true),"jpeg-input");
        if(heif){require(regular(jobpath+"/album.heic",true),"heif-input");if(!(stack||quarter))require(value(jobpath+"/heif.ready")=="HEIF_ENCODED 23326 17498 15","heif-marker");}
        auto plan=album_plan(value(jobpath+"/output-plan",128));std::string dir="/mnt/media_rw/ssd/DCIM/"+plan.folder,stem=dir+"/"+plan.image;
        if(!exists(dir))mkdir_new(dir);require(directory(dir),"album-directory");
        if(disk)require(value(jobpath+"/native.disk.ready")=="NATIVE_DISK_RAW_JPEG_DURABLE","disk-marker");
        if(memory||legacy)require(regular(jobpath+"/memory.ready")&&regular(jobpath+"/memory.inputs.ready"),"memory-markers");
        for(const auto&e:{"JPG","HEIC","3FR","raw-partial","stack-partial","partial","heif-partial"})require(!exists(stem+"."+e),"album-collision");
        require(same_device(jobpath,dir),"cross-device-publish");
        if(memory){
            step="source-archive";require(value(jobpath+"/sources.ready")=="SIX_NATIVE_SOURCE_RAWS_V1","source-ready");require(!exists(stem),"source-album-exists");
            for(unsigned i=0;i<6;i++)require(regular(jobpath+"/frame-"+std::to_string(i)+".3FR",true),"source-frame-missing");
            mkdir_new(stem);write_new(jobpath+"/sources.archive-started",stem+"\n");
            for(unsigned i=0;i<6;i++){std::string from=jobpath+"/frame-"+std::to_string(i)+".3FR",to=stem+"/frame-"+std::to_string(i)+".3FR";move_publish_new(from,to);require(!exists(from)&&regular(to,true),"source-move-verify");}
            write_new(jobpath+"/sources.archive-location",plan.folder+"/"+plan.image+"\n");puts("SIX_SOURCE_RAWS_RETAINED_IN_FINAL_RAW_NAMED_ALBUM_FOLDER");
        }
        auto begin=monotonic_ms();auto metric=[&](const std::string&name){auto now=monotonic_ms();append(jobpath+"/finish-metrics","PUBLISH_"+name+"_MS "+std::to_string(now-begin)+"\n");begin=now;};
        auto move=[&](const std::string&from,const std::string&to){require(same_device(from,dir),"cross-device-file");moved.push_back({to,from,false});move_publish_new(from,to);moved.back().active=true;require(!exists(from)&&regular(to,true),"move-verify");};
        auto rename=[&](const std::string&from,const std::string&to){std::string next=to;std::string *owner=nullptr;for(auto&m:moved)if(m.active&&m.current==from)owner=&m.current;for(auto&c:copied)if(c.active&&c.current==from)owner=&c.current;require(owner!=nullptr,"rename-ownership");move_publish_new(from,to);owner->swap(next);require(regular(to,true),"rename-verify");};
        step="stage-output";
        if(stack||quarter){
            move(jobpath+"/"+rawsource,stem+".raw-partial");move(jobpath+(heif?"/album.heic":"/album.jpg"),stem+".stack-partial");
            step="commit-output";rename(stem+".raw-partial",stem+".3FR");rename(stem+".stack-partial",stem+(heif?".HEIC":".JPG"));cancelled();sync();cancelled();committed=true;
        }else{
            move(jobpath+"/album.jpg",stem+".partial");if(raw)move(jobpath+"/final.3FR",stem+".raw-partial");
            if(heif){copied.push_back({stem+".heif-partial",false});copy_new(jobpath+"/album.heic",copied.back().current);copied.back().active=true;require(sha256(jobpath+"/album.heic")==sha256(copied.back().current),"heif-copy-verify");}
            metric("MOVE");if(heif)sync();step="commit-output";
            if(raw){rename(stem+".raw-partial",stem+".3FR");write_new(jobpath+"/raw.published",plan.image+".3FR\n");}
            if(heif)rename(stem+".heif-partial",stem+".HEIC");
            rename(stem+".partial",stem+".JPG");write_new(jobpath+"/jpeg.published",plan.image+".JPG\n");metric("RENAME");cancelled();sync();metric("FLUSH");cancelled();committed=true;
        }
        step="album-index";
        if(memory)memory_index(jobpath,plan);
        else if(stack)index_album(jobpath,plan);
        else if(quarter){browse(jobpath,plan);write_replace(jobpath+"/album-browse.log","NATIVE_ALBUM_BROWSE_OK\n");}
        else require(browse(jobpath,plan),"album-item-missing",1);
        if(!(stack||quarter))metric("BROWSE");
        if(raw)require(regular(stem+".3FR",true),"committed-raw-missing");
        require(regular(stem+((stack||quarter)&&heif?".HEIC":".JPG"),true),"committed-image-missing");
        write_new(jobpath+"/album-location",plan.folder+"/"+plan.image+((stack||quarter)&&heif?".HEIC\n":".JPG\n"));
        if(!(stack||quarter))puts("JPEG_AND_REQUESTED_NATIVE_FORMATS_ALBUM_ITEM_VERIFIED");return 0;
    }catch(const Error&e){primary=e.rc;cleanup_unconfirmed=e.recovery_failed;fprintf(stderr,"PUBLISH_PRIMARY step=%s detail=%s rc=%d errno=%d committed=%d\n",step.c_str(),e.what(),primary,e.system_errno,committed);}
     catch(const std::exception&e){primary=1;fprintf(stderr,"PUBLISH_PRIMARY step=%s detail=%s rc=1 committed=%d\n",step.c_str(),e.what(),committed);}
    bool recovery=cleanup_unconfirmed;Recovery recovery_scope;
    if(!committed){
        for(auto it=moved.rbegin();it!=moved.rend();++it){if(!it->active)continue;try{move_publish_new(it->current,it->source);}catch(const std::exception&e){recovery=true;fprintf(stderr,"PUBLISH_RESTORE_FAILED step=%s\n",e.what());}}
        for(const auto&p:copied){if(!p.active)continue;try{remove_regular(p.current);}catch(const std::exception&e){recovery=true;fprintf(stderr,"PUBLISH_COPY_CLEANUP_FAILED step=%s\n",e.what());}}
    }
    fprintf(stderr,"PUBLISH_FINISH primary_rc=%d restore_rc=%d committed=%d\n",primary,recovery?90:0,committed);return recovery?90:primary;
}
}
extern "C" int cn_publish_main(int argc,char **argv){return cn::publish_main(argc,argv);}
