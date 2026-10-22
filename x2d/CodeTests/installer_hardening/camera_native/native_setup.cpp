/* 固定 X2D 4.2.0 事务入口。旧机内脚本只作为历史对象，绝不执行。 */
#include "native_common.hpp"
#include "native_setup_tables.hpp"
#include "receipt_sha256.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <set>
#include <algorithm>
extern "C" int cn_signal_pending(void);
namespace cn { namespace setup {
static void syscall_ok(bool ok,const char*step,int rc=1){if(!ok)throw Error(step,rc,false,errno);}
struct Handle {
    int n;
    explicit Handle(int fd):n(fd){syscall_ok(n>=0,"setup-open");}
    ~Handle(){if(n>=0)close(n);}
    void release(){if(n>=0){int fd=n;n=-1;syscall_ok(!close(fd),"setup-close",90);}}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
};
static bool hex(const std::string&s,size_t n){return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static void component(const std::string&s){require(!s.empty()&&s!="."&&s!=".."&&s.find('/')==std::string::npos&&s.find('\0')==std::string::npos,"setup-component",2);}
static int subdir(int parent,const std::string&s,bool create=false){
    component(s);
    if(create&&mkdirat(parent,s.c_str(),0700))require(errno==EEXIST,"setup-create-directory");
    int fd=openat(parent,s.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    syscall_ok(fd>=0,"setup-directory-type");return fd;
}
static std::string content(int parent,const std::string&s,size_t limit=65536){
    component(s);Handle f(openat(parent,s.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));
    struct stat st;require(!fstat(f.n,&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"setup-file-type");
    std::string out;char b[4096];for(;;){ssize_t n=::read(f.n,b,sizeof b);if(n<0&&errno==EINTR)continue;require(n>=0,"setup-file-read");if(!n)break;require(out.size()+size_t(n)<=limit,"setup-file-limit");out.append(b,size_t(n));}
    require(out.find('\0')==std::string::npos,"setup-file-nul");return out;
}
static std::string digest(int parent,const std::string&s,struct stat*identity=nullptr){
    component(s);Handle f(openat(parent,s.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));struct stat st;
    require(!fstat(f.n,&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"setup-hash-type");
    ReceiptSha sha;rsha_init(&sha);unsigned char b[65536];for(;;){cancelled();ssize_t n=::read(f.n,b,sizeof b);if(n<0&&errno==EINTR)continue;require(n>=0,"setup-hash-read");if(!n)break;rsha_update(&sha,b,unsigned(n));}
    struct stat after;require(!fstat(f.n,&after)&&st.st_size==after.st_size&&st.st_mtim.tv_sec==after.st_mtim.tv_sec&&st.st_mtim.tv_nsec==after.st_mtim.tv_nsec,"setup-hash-changed");
    unsigned char raw[32];rsha_final(&sha,raw);std::string out;for(auto v:raw){out+="0123456789abcdef"[v>>4];out+="0123456789abcdef"[v&15];}if(identity)*identity=st;return out;
}
static bool present(int parent,const std::string&s){struct stat st;component(s);if(!fstatat(parent,s.c_str(),&st,AT_SYMLINK_NOFOLLOW))return true;require(errno==ENOENT,"setup-file-state");return false;}
static void unlink_owned(int parent,const std::string&s,const struct stat&owner,int flags=0){
    struct stat st;require(!fstatat(parent,s.c_str(),&st,AT_SYMLINK_NOFOLLOW)&&st.st_dev==owner.st_dev&&st.st_ino==owner.st_ino,"setup-object-replaced",90);
    syscall_ok(!unlinkat(parent,s.c_str(),flags),"setup-unlink",90);
}
static void receipt(int dir,const std::string&name,const std::string&text){
    component(name);const std::string pending=name+".native-next";
    Handle f(openat(dir,pending.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));struct stat owner;require(!fstat(f.n,&owner),"setup-receipt-stat");
    try{
        size_t offset=0;while(offset<text.size()){ssize_t n=write(f.n,text.data()+offset,text.size()-offset);if(n<0&&errno==EINTR)continue;syscall_ok(n>0,"setup-receipt-write");offset+=size_t(n);}syscall_ok(!fsync(f.n),"setup-receipt-sync");
        if(present(dir,name)){struct stat st;require(!fstatat(dir,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"setup-receipt-target");}
        syscall_ok(!renameat(dir,pending.c_str(),dir,name.c_str()),"setup-receipt-rename");syscall_ok(!fsync(dir),"setup-receipt-directory-sync");
    }catch(const std::exception&e){
        fprintf(stderr,"SETUP_RECEIPT_PRIMARY detail=%s\n",e.what());
        if(present(dir,pending)){try{unlink_owned(dir,pending,owner);}catch(const std::exception&cleanup){
            const auto*original=dynamic_cast<const Error*>(&e);fprintf(stderr,"SETUP_RECEIPT_CLEANUP_UNCONFIRMED detail=%s restore_rc=90\n",cleanup.what());
            throw Error(e.what(),original?original->rc:1,true,original?original->system_errno:0);
        }}
        throw;
    }
}
static void idle(){
    require(get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("system","system_state","int32")=="1","setup-not-idle");
}
static unsigned long mount_flags(bool readonly){
    std::istringstream lines(read("/proc/self/mounts",1024*1024));std::string line;unsigned count=0;unsigned long flags=0;
    while(std::getline(lines,line)){
        std::istringstream row(line);std::string source,target,type,options;require(bool(row>>source>>target>>type>>options),"setup-mount-record");if(target!="/system")continue;++count;
        std::istringstream option_stream(options);std::string option;while(std::getline(option_stream,option,',')){
            if(option=="nosuid")flags|=MS_NOSUID;else if(option=="nodev")flags|=MS_NODEV;else if(option=="noexec")flags|=MS_NOEXEC;
            else if(option=="sync")flags|=MS_SYNCHRONOUS;else if(option=="dirsync")flags|=MS_DIRSYNC;else if(option=="noatime")flags|=MS_NOATIME;
            else if(option=="nodiratime")flags|=MS_NODIRATIME;else if(option=="relatime")flags|=MS_RELATIME;else if(option=="strictatime")flags|=MS_STRICTATIME;
        }
    }
    require(count==1,"setup-system-mount-ambiguous");return flags|MS_REMOUNT|(readonly?MS_RDONLY:0);
}
static void remount(bool readonly){
    auto flags=mount_flags(readonly);syscall_ok(!system_remount(flags),"setup-system-remount",90);
    std::istringstream rows(read("/proc/self/mounts",1024*1024));std::string a,b,c,d;bool found=false;
    while(rows>>a>>b>>c>>d){std::string rest;std::getline(rows,rest);if(b=="/system"){std::string opts=","+d+",";require(opts.find(readonly?",ro,":",rw,")!=std::string::npos,"setup-system-remount-verify",90);found=true;}}
    require(found,"setup-system-mount-lost",90);
}
struct Retained {std::string id,hash;struct stat identity{};bool exists=false;};
static void no_mappings(const std::vector<Retained>&objects,const std::string&path){
    DIR*proc=opendir("/proc");require(proc!=nullptr,"setup-proc-open");
    try{
        for(;;){
            errno=0;auto*item=readdir(proc);if(!item){require(!errno,"retained-proc-enumeration-unconfirmed",5);break;}
            if(!digits(item->d_name))continue;std::string prefix=std::string("/proc/")+item->d_name;
            try{
                auto maps=read(prefix+"/maps",16*1024*1024);require(maps.find(path+"/")==std::string::npos,"retained-still-mapped",5);
                std::istringstream lines(maps);std::string line;while(std::getline(lines,line)){
                    std::istringstream row(line);std::string address,permissions,offset,device,inode;require(bool(row>>address>>permissions>>offset>>device>>inode),"setup-maps-record",5);
                    auto colon=device.find(':');require(colon!=std::string::npos&&digits(inode),"setup-maps-device",5);
                    size_t used=0;unsigned long major_id=std::stoul(device.substr(0,colon),&used,16);require(used==colon,"setup-maps-major",5);
                    auto minor_text=device.substr(colon+1);unsigned long minor_id=std::stoul(minor_text,&used,16);require(used==minor_text.size(),"setup-maps-minor",5);
                    auto inode_id=std::stoull(inode);
                    for(const auto&o:objects)if(o.exists&&o.identity.st_ino==inode_id&&major(o.identity.st_dev)==major_id&&minor(o.identity.st_dev)==minor_id)throw Error("retained-inode-still-mapped",5);
                }
            }catch(const Error&e){
                if(e.rc==5)throw;
                // 只有进程已经消失才可忽略；权限/解析/读取错误都不是“无引用”。
                if(!access(prefix.c_str(),F_OK)||errno!=ENOENT)throw Error("retained-maps-unconfirmed",5);
            }
        }
    }catch(...){closedir(proc);throw;}closedir(proc);
}
static std::string suffix(const std::string&path){const std::string prefix="/data/local/tmp/x2d-un-";require(path.rfind(prefix,0)==0&&hex(path.substr(prefix.size()),8),"setup-uninstall-stage",2);return path.substr(prefix.size());}
static int stage_init(const std::string&name){
    require(name=="/data/local/tmp/x2d-rs1"||name=="/data/local/tmp/x2d-rs8"||name.rfind("/data/local/tmp/x2d-un-",0)==0||name.rfind("/data/local/tmp/x2d-tidy-",0)==0,"setup-stage-name",2);
    if(name.rfind("/data/local/tmp/x2d-un-",0)==0)suffix(name);
    if(name.rfind("/data/local/tmp/x2d-tidy-",0)==0)require(hex(name.substr(std::string("/data/local/tmp/x2d-tidy-").size()),8),"setup-tidy-stage",2);
    Handle root(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));Handle data(subdir(root.n,"data"));Handle local(subdir(data.n,"local",true));Handle tmp(subdir(local.n,"tmp",true));Handle stage(subdir(tmp.n,name.substr(name.find_last_of('/')+1),true));
    puts("NATIVE_STAGE_DIRECTORY_READY");return 0;
}
static int retained_cleanup(const std::string&path,const std::string&attempt){
    require(hex(attempt,16),"setup-attempt-id",2);
    const auto id=suffix(path),retained_name="x2d-un-retained-"+id,retained_path="/system/"+retained_name;
    Handle root(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));Handle data(subdir(root.n,"data"));Handle local(subdir(data.n,"local"));Handle tmp(subdir(local.n,"tmp"));Handle stage(subdir(tmp.n,"x2d-un-"+id));Handle backup(subdir(stage.n,"backup"));Handle system(subdir(root.n,"system"));Handle retained(subdir(system.n,retained_name));
#ifdef CN_PACKAGED_PAYLOAD
    if(present(stage.n,"payload")){
        Handle payload(subdir(stage.n,"payload"));
        require(digest(payload.n,"targets.txt")==legacy_targets_sha&&digest(payload.n,"guards.txt")==legacy_guards_sha,"setup-legacy-table-hash",3);
    }else{
        // 仅兼容已审计的 H3 旧根目录表，不执行其 cleanup.sh，也不信任任意旧表。
        require(digest(stage.n,"targets.txt")==legacy_compat_targets_sha&&digest(stage.n,"guards.txt")==legacy_guards_sha,"setup-compat-table-hash",3);
    }
#else
    require(digest(stage.n,"targets.txt")==legacy_targets_sha&&digest(stage.n,"guards.txt")==legacy_guards_sha,"setup-legacy-table-hash",3);
#endif
    auto result=trim_newlines(content(stage.n,"result",16));require(digits(result),"setup-legacy-result",3);
    auto previous=trim_newlines(content(stage.n,"boot.sha256",128));require(hex(previous,64),"setup-boot-digest",3);
    require(previous!=sha256("/proc/sys/kernel/random/boot_id"),"retained-manual-reboot-required",4);idle();
    for(const auto*name:{"cleanup.result","cleanup.done","cleanup.final"}){
        require(!present(stage.n,std::string(name)+".native-next"),"setup-receipt-pending-conflict",3);
        if(present(stage.n,name))content(stage.n,name,4096);
    }
    std::vector<Retained> objects;std::set<std::string> ids;std::istringstream rows(content(stage.n,"retained.txt",65536));std::string line;
    while(std::getline(rows,line)){
        std::istringstream row(line);Retained object;std::string extra;require(bool(row>>object.id>>object.hash)&&!(row>>extra)&&hex(object.hash,64),"retained-record",3);
        require(ids.insert(object.id).second&&ids.size()<=sizeof(legacy_targets)/sizeof(legacy_targets[0]),"retained-duplicate-or-limit",3);
        const LegacyTarget* target=nullptr;for(const auto&t:legacy_targets)if(object.id==t.id){target=&t;break;}require(target!=nullptr,"retained-unknown-id",3);
        const std::string allowed=target->allowed;require(allowed.find(","+object.hash+",")!=std::string::npos||allowed.find(",PROJECT_FILE,")!=std::string::npos,"retained-digest-not-allowed",3);
        require(trim_newlines(content(backup.n,object.id+".sha",128))==object.hash,"retained-backup-mismatch",3);
        object.exists=present(retained.n,object.id);if(object.exists)require(digest(retained.n,object.id,&object.identity)==object.hash,"retained-content-mismatch",3);objects.push_back(object);
    }
    require(!objects.empty(),"retained-empty-table",3);
    DIR*entries=fdopendir(dup(retained.n));require(entries!=nullptr,"retained-directory-read");
    try{for(;;){errno=0;auto*item=readdir(entries);if(!item){require(!errno,"retained-enumeration-unconfirmed",3);break;}std::string name=item->d_name;if(name=="."||name=="..")continue;require(ids.count(name)!=0,"retained-unknown-entry",3);struct stat st;require(!fstatat(retained.n,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"retained-entry-type",3);}}catch(...){closedir(entries);throw;}closedir(entries);
    no_mappings(objects,retained_path);cancelled();
    struct stat retained_identity;require(!fstat(retained.n,&retained_identity),"retained-directory-identity");
    Handle runtime(subdir(root.n,"tmp"));
    // 所有内容验证后才取得本次锁；已有锁不删除、不接管。
    syscall_ok(!mkdirat(tmp.n,"x2d-native-setup-lock",0700),"setup-lock-busy");struct stat lock{},task_lock{};
    int primary=0,recovery=0,primary_errno=0;std::string primary_step="none";bool changed=false,lock_identified=false,task_acquired=false,task_identified=false;
    try{
        require(!fstatat(tmp.n,"x2d-native-setup-lock",&lock,AT_SYMLINK_NOFOLLOW)&&S_ISDIR(lock.st_mode),"setup-lock-stat",90);lock_identified=true;
        syscall_ok(!mkdirat(runtime.n,"x2d-multishot-active",0700),"setup-camera-task-lock-busy");task_acquired=true;
        require(!fstatat(runtime.n,"x2d-multishot-active",&task_lock,AT_SYMLINK_NOFOLLOW)&&S_ISDIR(task_lock.st_mode),"setup-camera-task-lock-stat",90);task_identified=true;
        idle();no_mappings(objects,retained_path);puts("RETAINED_FILES_PRECHECK_COMPLETE");changed=true;remount(false);
        for(const auto&o:objects){cancelled();if(!present(retained.n,o.id))continue;struct stat now;require(digest(retained.n,o.id,&now)==o.hash,"retained-changed-before-delete",3);require(o.exists&&now.st_dev==o.identity.st_dev&&now.st_ino==o.identity.st_ino,"retained-identity-changed",3);unlink_owned(retained.n,o.id,o.identity);}
        unlink_owned(system.n,retained_name,retained_identity,AT_REMOVEDIR);require(!fsync(system.n),"retained-directory-sync");
    }catch(const Error&e){primary=e.rc;primary_step=e.what();primary_errno=e.system_errno;if(e.recovery_failed)recovery=90;fprintf(stderr,"SETUP_PRIMARY step=%s rc=%d\n",e.what(),primary);}
    catch(const std::exception&e){primary=1;primary_step="native-exception";fprintf(stderr,"SETUP_PRIMARY detail=%s rc=1\n",e.what());}
    {
        Recovery finish;
        // 已 unlink 的目录仍被打开时，Linux 会拒绝将所在挂载回只读（EBUSY）。
        try{retained.release();}catch(const std::exception&e){recovery=90;fprintf(stderr,"SETUP_RETAINED_CLOSE_UNCONFIRMED detail=%s\n",e.what());}
        if(changed)try{remount(true);}catch(const std::exception&e){recovery=90;fprintf(stderr,"SETUP_READONLY_UNCONFIRMED detail=%s\n",e.what());}
        if(!recovery&&task_acquired)try{require(task_identified,"setup-camera-task-lock-identity-unconfirmed",90);unlink_owned(runtime.n,"x2d-multishot-active",task_lock,AT_REMOVEDIR);}catch(const std::exception&e){recovery=90;fprintf(stderr,"SETUP_TASK_UNLOCK_UNCONFIRMED detail=%s\n",e.what());}
        if(!recovery)try{require(lock_identified,"setup-lock-identity-unconfirmed",90);unlink_owned(tmp.n,"x2d-native-setup-lock",lock,AT_REMOVEDIR);}catch(const std::exception&e){recovery=90;fprintf(stderr,"SETUP_UNLOCK_UNCONFIRMED detail=%s\n",e.what());}
        if(recovery)fprintf(stderr,"SETUP_RECOVERY_UNCONFIRMED_LOCKS_RETAINED\n");
        if(!primary&&cn_signal_pending()){primary=143;primary_step="cancelled-during-finish";}
        // 最终回执在只读和锁收尾之后；兼容标记不能单独证明成功。
        try{
            receipt(stage.n,"cleanup.result",std::to_string(recovery?recovery:primary)+"\n");
            if(!primary&&!recovery)receipt(stage.n,"cleanup.done","RETAINED_PATCH_FILES_CLEANED\n");
            receipt(stage.n,"cleanup.final","NATIVE_TRANSACTION_V1\noperation=retained-cleanup\ntransaction="+id+"\nattempt="+attempt+"\nprimary_step="+primary_step+"\nprimary_errno="+(primary_errno?std::to_string(primary_errno):"UNAVAILABLE")+"\nprimary_rc="+std::to_string(primary)+"\nrestore_rc="+std::to_string(recovery)+"\nfinal_rc="+std::to_string(recovery?recovery:primary)+"\n");
        }catch(const std::exception&e){recovery=90;fprintf(stderr,"SETUP_RECEIPT_UNCONFIRMED detail=%s\n",e.what());}
    }
    fprintf(stderr,"SETUP_FINISH primary_rc=%d restore_rc=%d\n",primary,recovery);return recovery?recovery:primary;
}
} }
extern "C" int cn_setup_main(int argc,char**argv){
    try{cn::require(argc>=2,"setup-arguments",2);cn::cancelled();const std::string action=argv[1];if(action=="stage-init"){cn::require(argc==3,"setup-arguments",2);cn::TransactionGuard transaction;return cn::setup::stage_init(argv[2]);}if(action=="retained-cleanup"){cn::require(argc==4,"setup-arguments",2);cn::TransactionGuard transaction;return cn::setup::retained_cleanup(argv[2],argv[3]);}throw cn::Error("setup-command",2);}
    catch(const cn::Error&e){fprintf(stderr,"SETUP_REFUSED step=%s rc=%d\n",e.what(),e.rc);return e.recovery_failed?90:e.rc;}
    catch(const std::exception&e){fprintf(stderr,"SETUP_REFUSED detail=%s rc=1\n",e.what());return 1;}
}
