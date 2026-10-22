/* p41 原生事务：保持 before/after、有限等待、持久计数/缓存双重确认。 */
#include "native_common.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" void cn_recovery_mode(int);
namespace cn {
int counter_main(int argc,char **argv){
    std::string step="ARGUMENTS",path,phase,old_file,old_folder;
    bool stopped=false,changed=false,committed=false;int primary=0;
    try{
        require(argc==4,"ARGUMENTS",2);path=argv[2];phase=argv[3];
        step="JOB_PATH";job(path);
        step="JOB_LOCK";require(directory("/tmp/x2d-multishot-active"),step);
        step="OUTPUT_PLAN";auto plan=album_plan(value(path+"/output-plan",128));
        step="COUNTER_PHASE";require(phase=="before"||phase=="after",step,2);
        require(phase!="before"||plan.file_number>0,"before-counter-underflow",2);
        std::string file=std::to_string(plan.file_number-(phase=="before"?1:0));
        std::string folder=std::to_string(plan.folder_number);
        auto wait_env=getenv("X2D_COUNTER_WAIT_IDLE");
        bool wait=phase=="before"&&wait_env&&!strcmp(wait_env,"1");
        unsigned timeout=wait?500:3000,stable=0,count=0;uint64_t begin=monotonic_ms();std::string previous;
        for(;;){
            cancelled();step="EXPOSURE_IDLE";auto exposure=get("camera","exposure_status","int32",timeout);require(digits(exposure),"COUNTER_IDLE_INVALID_EXPOSURE");
            step="STORAGE_IDLE";auto storage=get("storage","storage_status","int32",timeout);require(digits(storage),"COUNTER_IDLE_INVALID_STORAGE");
            step="NATIVE_ERROR_CLEAR";auto error=get("error","active_error","boolean",timeout);require(error=="true"||error=="false","COUNTER_IDLE_INVALID_ERROR");
            uint64_t elapsed=monotonic_ms()-begin;count++;
            std::string snapshot=exposure+":"+storage+":"+error;
            if(snapshot!=previous){printf("COUNTER_IDLE_STATE phase=%s exposure=%s storage=%s error=%s elapsed_ms=%llu\n",phase.c_str(),exposure.c_str(),storage.c_str(),error.c_str(),static_cast<unsigned long long>(elapsed));previous=snapshot;}
            require(error=="false",step);
            stable=exposure=="0"&&storage=="0"?stable+1:0;
            step=exposure!="0"?"EXPOSURE_IDLE":storage!="0"?"STORAGE_IDLE":"STABLE_IDLE";
            require(!(wait&&elapsed>=5000),"COUNTER_IDLE_WAIT_EXHAUSTED");
            if(stable>=3||(!wait&&stable==1))break;
            require(wait&&elapsed<5000&&count<51,"COUNTER_IDLE_WAIT_EXHAUSTED");sleep_ms(100);
        }
        step="PERSISTENT_COUNTERS";old_file=getprop("persist.hbl.dcf.fileindex");old_folder=getprop("persist.hbl.dcf.folderindex");
        step="SERVICE_COUNTER";
        if(old_file==file&&old_folder==folder&&get("storage","camera_file_index","int32")==file){printf("NATIVE_COUNTER_ALREADY_SYNCHRONIZED_%s\n",phase.c_str());return 0;}
        step="STOP_STORAGE";stopped=true;service("camera-storage",false);
        step="WAIT_STORAGE_EXIT";unsigned i=0;while(!pids("camera-storage").empty()){require(++i<40,step);sleep_ms(100);}
        changed=true;
        step="SET_FILE_COUNTER";setprop("persist.hbl.dcf.fileindex",file);
        step="SET_FOLDER_COUNTER";setprop("persist.hbl.dcf.folderindex",folder);
        step="VERIFY_FILE_COUNTER";require(getprop("persist.hbl.dcf.fileindex")==file,step);
        step="VERIFY_FOLDER_COUNTER";require(getprop("persist.hbl.dcf.folderindex")==folder,step);
        step="START_STORAGE";service("camera-storage",true);
        step="WAIT_SERVICE_COUNTER";i=0;
        for(;;){
            bool ready=false;try{ready=get("storage","camera_file_index","int32")==file&&get("storage","storage_status","int32")=="0";}catch(const Error&e){if(e.rc==143)throw;}
            if(ready)break;require(++i<30,step);sleep_ms(1000);
        }
        step="NATIVE_ERROR_AFTER_SYNC";require(get("error","active_error","boolean")=="false",step);
        committed=true;stopped=false;printf("NATIVE_COUNTER_SYNCHRONIZED_%s\n",phase.c_str());return 0;
    }catch(const Error&e){primary=e.rc;fprintf(stderr,"COUNTER_SYNC_PRIMARY step=%s detail=%s rc=%d\n",step.c_str(),e.what(),primary);}
     catch(const std::exception&e){primary=1;fprintf(stderr,"COUNTER_SYNC_PRIMARY step=%s detail=%s rc=1\n",step.c_str(),e.what());}
    bool failed_restore=false;
    Recovery recovery_scope;
    auto restore_action=[&](auto action){try{action();}catch(const std::exception&e){failed_restore=true;fprintf(stderr,"COUNTER_RECOVERY_FAILED detail=%s\n",e.what());}};
    if(!committed&&changed){
        restore_action([&](){service("camera-storage",false);});
        restore_action([&](){setprop("persist.hbl.dcf.fileindex",old_file);});
        restore_action([&](){setprop("persist.hbl.dcf.folderindex",old_folder);});
        restore_action([&](){require(getprop("persist.hbl.dcf.fileindex")==old_file,"restore-file-counter");});
        restore_action([&](){require(getprop("persist.hbl.dcf.folderindex")==old_folder,"restore-folder-counter");});
    }
    if(stopped){
        restore_action([&](){service("camera-storage",true);});
        restore_action([&](){
            bool ready=false;
            for(unsigned i=0;i<10;i++){
                try{ready=get("storage","camera_file_index","int32")==old_file&&get("storage","storage_status","int32")=="0";}catch(const Error&){}
                if(ready)break;sleep_ms(100);
            }
            require(ready,"restore-service-counter");
        });
    }
    if(failed_restore){
        try{write_new(path+"/counter-restore.failed","COUNTER_RECOVERY_UNCONFIRMED\n");}catch(const std::exception&){}
        fprintf(stderr,"COUNTER_RECOVERY_UNCONFIRMED\n");
    }
    int result=failed_restore?90:primary;
    fprintf(stderr,"COUNTER_SYNC_FAILED step=%s primary_rc=%d recovery_failed=%d rc=%d\n",step.c_str(),primary,failed_restore,result);
    return result;
}
}
extern "C" int cn_counter_main(int argc,char **argv){return cn::counter_main(argc,argv);}
