/* X2D 4.2.0 冻结候选的内存显影与十六帧显影编排。无解释器。 */
#include "native_process.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
namespace cn {
static const std::string ipc="/tmp/x2d-pixel-overclock/memory.sock";
static const std::string lease="/tmp/x2d-pixel-overclock/render-lease";
static const std::string accepted="/tmp/x2d-pixel-overclock/render-accepted";
static void absent_leases(){for(const auto&p:{lease,lease+".tmp",accepted,accepted+".tmp"})require(!exists(p),"render-lease-exists");}
static void exposure_idle(){require(get("camera","exposure_status","int32")=="0","render-exposure-active");require(get("error","active_error","boolean")=="false","render-native-error");}
static void stream_idle(){require(get("camera","stream_group","int32")=="255","render-stream-active",90);}
static void hash(const std::string&p,const std::string&expected,int rc=1){require(sha256(p)==expected,"render-binary-hash",rc);}
static void durable_raw(const std::string&s){
    struct stat st;require(!lstat((s+"/merged.3FR").c_str(),&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_size>0&&st.st_size%4096==0,"sixteen-merged-raw-not-durable");
}
static int memory_render(const std::string&s,bool legacy){
    const std::string tool=legacy?"/system/bin/x2d-memory-manual-r17/render-probe":"/system/bin/x2d-pixel-overclock/render-memory";
    const std::string probe=s+"/probe.log";
    job(s);cancelled();require(regular(s+"/memory.inputs.ready"),"memory-inputs-ready");
    for(const auto&p:{s+"/memory.sock",ipc,s+"/render.lock",s+"/config.ready",s+"/config.ready.tmp",s+"/render.restored",s+"/render.drained",s+"/render.cleaned",s+"/render.jpg",probe,s+"/probe.pid",s+"/render.started"})require(!exists(p),"render-existing-output");
    mkdir_new(s+"/render.lock");absent_leases();require(value("/tmp/x2d-pixel-render-route.status")=="READY","render-route");
    hash(tool,legacy?"fc58a4fe777ed757cf295cf4c56f300f87eb2d176941e8df6f8130ac71236434":"fdb428860cd7c04c673b523201520230da5efc9efdbd613100c37a328e42b32b",74);
    hash("/system/etc/imx461bqr_ec1706.conf","e9eeb3630b1eef7a486957c0c2b7fc4e33f2288fd5955a09d2aa0866d9cfd700");
    hash("/system/lib64/camera/plugins/lib_frog_e2_ec1706_native.so","598ff042ae5cd4514dae56a9ecba3b5d52b92924c248b4bcfa74fa1ee2cbe4dd");
    exposure_idle();require(get("camera","live_view_state","int32")=="0","render-live-active");stream_idle();
    Child child;int primary=0;bool cleanup_unconfirmed=false;
    try{
        write_new(s+"/render.started","");child.start({tool,s+"/memory-source",s+"/memory.sock",s+"/config.ready"},probe);
        write_new(s+"/probe.pid",std::to_string(child.pid())+"\n");
        unsigned i=0;
        while(!log_count(probe,"MEMORY_NATIVE_LISTENER_READY")){
            if(child.poll()){int rc=child.wait();throw Error("render-listener-early-exit",rc?rc:72);}
            require(++i<50,"render-listener-timeout",73);sleep_ms(1000);
        }
        cancelled();
        // 固定内存渲染器在 run_memory_native 中直接运行，不消费旧配置握手。
        // 流空闲必须在启动它之前检查；启动后可能已进入真实原厂渲染。
        if(legacy){stream_idle();write_new(s+"/config.ready.tmp","408\n");move_new(s+"/config.ready.tmp",s+"/config.ready");}
        int rc=child.wait();cancelled();require(!rc,"render-child",rc);
        auto native=saved_count(probe,"NATIVE_JPEG_SAVED bytes="),software=saved_count(probe,"SOFTWARE_JPEG_SAVED bytes="),complete=log_count(probe,"JPEG_SOFTWARE_FALLBACK_COMPLETE");
        require((native==1&&!software&&!complete)||(!native&&software==1&&complete==1),"MEMORY_JPEG_SUCCESS_ROUTE_REJECTED",76);
        require(!log_prefix(probe,"SOFTWARE_JPEG_FAILURE CODE "),"software-jpeg-failure",76);
        require(log_count(probe,"OBSERVER_EXIT 0")>0&&regular(s+"/render.jpg",true),"render-output");
        puts("NATIVE_BRANCH_MAIN_AND_PREVIEW_VERIFIED_WITHOUT_SERVICE_RESTART");
    }catch(const Error&e){primary=e.rc;cleanup_unconfirmed=e.recovery_failed;fprintf(stderr,"MEMORY_RENDER_PRIMARY step=%s rc=%d\n",e.what(),primary);}
    catch(const std::exception&e){primary=1;fprintf(stderr,"MEMORY_RENDER_PRIMARY detail=%s rc=1\n",e.what());}
    // 保留主失败；不提前杀显影器。只有等待退出和原厂流空闲后才能发 drain 回执。
    Recovery recovery;
    try{
        require(child.pid()>0,"render-child-not-created",90);int rc=child.wait();if(!primary&&rc)primary=rc;
        require(log_count(probe,"MEMORY_RENDER_RESOURCES_DRAINED")==1,"render-drain-receipt",90);stream_idle();
        require(value(s+"/probe.pid")==std::to_string(child.pid()),"render-probe-owner",90);
        write_new(s+"/render.drained","RENDER_DRAINED_V1\n");sync();
        if(exists(ipc)){
            struct stat st;require(!lstat(ipc.c_str(),&st)&&S_ISSOCK(st.st_mode),"render-endpoint-type",90);
            require(log_count(probe,"MEMORY_NATIVE_LISTENER_READY")>0,"render-listener-receipt",90);require(!unlink(ipc.c_str()),"render-endpoint-remove",90);
        }
        exposure_idle();lease_remove(lease,child.pid());lease_remove(accepted,child.pid());
        require(!exists(lease+".tmp")&&!exists(accepted+".tmp"),"render-temporary-leases",90);
        write_new(s+"/render.cleaned","RENDER_CLEANED_V1\n");sync();
        if(!primary&&!cleanup_unconfirmed)write_new(s+"/render.restored","");
    }catch(const std::exception&e){fprintf(stderr,"MEMORY_RENDER_CLEANUP_UNCONFIRMED detail=%s\n",e.what());cleanup_unconfirmed=true;}
    fprintf(stderr,"MEMORY_RENDER_FINISH primary_rc=%d restore_rc=%d\n",primary,cleanup_unconfirmed?90:0);
    return cleanup_unconfirmed?90:primary;
}
static void run(const std::vector<std::string>&args,const std::string&log){Child child;child.start(args,log);int rc=child.wait();cancelled();require(!rc,"pipeline-child",rc);}
static void sixteen_release(Child&child){
    if(child.pid()<=0)return;
    if(!child.poll()){
        require(executable(child.pid())=="/system/bin/x2d-multi-resolution/render16","sixteen-render-owner",90);
        if(get("camera","stream_group","int32")!="255"){
            auto data=read(lease);require(data.size()>=12,"sixteen-lease-size",90);auto*b=reinterpret_cast<const unsigned char*>(data.data());
            uint32_t owner=uint32_t(b[8])|uint32_t(b[9])<<8|uint32_t(b[10])<<16|uint32_t(b[11])<<24;
            require(owner==uint32_t(child.pid()),"sixteen-lease-owner",90);
            auto r=command({"/bin/dbus-send","--system","--print-reply","--reply-timeout=20000","--dest=com.hasselblad.camera","/camera","com.hasselblad.camera.reprocess_stop_session"},21000);
            require(!r.rc,"sixteen-stop-session",90);unsigned i=0;
            while(get("camera","stream_group","int32")!="255"){require(++i<50,"sixteen-stop-stream-timeout",90);sleep_ms(200);}
        }
        child.terminate_after_release("/system/bin/x2d-multi-resolution/render16");
    }
    stream_idle();lease_remove(lease,child.pid());lease_remove(accepted,child.pid());
}
static int sixteen_render(const std::string&s){
    job(s);cancelled();require(regular(s+"/frames.ready")&&value(s+"/capture.requested")=="16"&&regular(s+"/rawpb.bin"),"sixteen-inputs");
    mkdir_new(s+"/sixteen.pipeline.lock");for(const auto&p:{"merged.3FR","render.jpg","album.jpg"})require(!exists(s+"/"+p),"sixteen-existing-output");
    const std::string tools="/system/bin/x2d-multi-resolution/";std::string phase="merge";Child render(false);bool released=false,cleanup_unconfirmed=false;int primary=0;
    auto stage=[&](const std::string&p){write_replace(s+"/stage",p+"\n");};
    try{
        stage("merge");run({tools+"merge16",s+"/merged.3FR",s,"1024","512"},s+"/merge.log");
        require(log_count(s+"/merge.log","SIXTEEN_DISK_RAW_FSYNC_4K_COMPLETE_INPUTS_RETAINED")>0,"sixteen-merge-receipt");
        durable_raw(s);
        write_new(s+"/merged.ready","SIXTEEN_MERGED_RAW_DURABLE\n");
        for(unsigned i=0;i<16;i++)require(regular(s+"/frame-"+std::to_string(i)+".3FR"),"sixteen-frame-type");
        for(unsigned i=0;i<16;i++){cancelled();remove_regular(s+"/frame-"+std::to_string(i)+".3FR");}write_new(s+"/sources.cleaned","");
        phase="render";stage("render");require(value("/tmp/x2d-pixel-render-route.status")=="READY","sixteen-render-route");absent_leases();
        require(get("camera","exposure_status","int32")=="0"&&get("camera","live_view_state","int32")=="0","sixteen-render-idle");
        render.start({tools+"render16","--sixteen-disk",s},s+"/probe.log");int rc=render.wait(true);cancelled();require(!rc,"sixteen-render-child",rc);sixteen_release(render);released=true;
        require(log_count(s+"/probe.log","SIXTEEN_25_NATIVE_TILES_COMPLETE_NO_FULL_YUV")>0,"sixteen-tiles-receipt");write_new(s+"/sixteen.tiles.ready","");
        phase="jpeg-join";stage("jpeg_join");run({tools+"jpeg-grid",s+"/render.jpg",s,"46652","34996","11648","8736"},s+"/jpeg-join.log");
        phase="package";stage("package");run({"/system/bin/x2d-pixel-overclock/preview",s+"/merged.3FR",s+"/preview.jpg"},s+"/preview.log");
        run({tools+"pack16",s+"/render.jpg",s+"/preview.jpg",s+"/album.jpg","--direct-staged-output","--capture-raw",s+"/merged.3FR"},s+"/package.log");
        struct stat st;require(regular(s+"/album.jpg",true)&&!lstat((s+"/album.jpg").c_str(),&st)&&st.st_size%4096==0,"sixteen-album-alignment");write_new(s+"/sixteen.output.ready","SIXTEEN_RAW_AND_JPEG_DURABLE\n");
        for(unsigned i=0;i<25;i++)require(regular(s+"/render-tile-"+std::to_string(i)+".jpg"),"sixteen-tile-type");
        for(unsigned i=0;i<25;i++){cancelled();remove_regular(s+"/render-tile-"+std::to_string(i)+".jpg");}write_new(s+"/sixteen.tiles.cleaned","");
        durable_raw(s);phase="complete";puts("SIXTEEN_FINAL_RAW_JPEG_READY_NOT_ALBUM_PUBLISHED");
    }catch(const Error&e){primary=e.rc;cleanup_unconfirmed=e.recovery_failed;fprintf(stderr,"SIXTEEN_PIPELINE_PRIMARY phase=%s detail=%s rc=%d\n",phase.c_str(),e.what(),primary);}
    catch(const std::exception&e){primary=1;fprintf(stderr,"SIXTEEN_PIPELINE_PRIMARY phase=%s detail=%s rc=1\n",phase.c_str(),e.what());}
    Recovery recovery;int final=cleanup_unconfirmed?90:primary;
    if(!released){try{sixteen_release(render);}catch(const std::exception&e){final=90;fprintf(stderr,"SIXTEEN_PIPELINE_RESTORE detail=%s primary_rc=%d\n",e.what(),primary);}}
    fprintf(stderr,"SIXTEEN_PIPELINE_FINISH phase=%s rc=%d\n",phase.c_str(),final);return final;
}
int render_main(int argc,char**argv){
    try{require(argc==3,"render-arguments",2);std::string mode=argv[1];
        if(mode=="render-memory")return memory_render(argv[2],false);
        if(mode=="render-legacy-memory")return memory_render(argv[2],true);
        if(mode=="render-sixteen")return sixteen_render(argv[2]);
        throw Error("render-mode",2);
    }catch(const Error&e){fprintf(stderr,"RENDER_FAILURE step=%s rc=%d\n",e.what(),e.rc);return e.rc;}
    catch(const std::exception&e){fprintf(stderr,"RENDER_FAILURE detail=%s rc=1\n",e.what());return 1;}
}
}
extern "C" int cn_render_main(int argc,char**argv){return cn::render_main(argc,argv);}
