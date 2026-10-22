/* 初次合法 /system/bin 引导之后的原生安全切换；无脚本/任意路径入口。 */
#include "native_common.hpp"
#include "receipt_sha256.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>
extern "C" int cn_signal_pending(void);
namespace cn { namespace bootstrap {
struct FD {int n;explicit FD(int v):n(v){}~FD(){if(n>=0)close(n);}FD(const FD&)=delete;};
static void ok(bool v,const char*s,int rc=1){if(!v)throw Error(s,rc,false,errno);}
static bool hex(const std::string&s,size_t n){return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static int directory(const std::string&path){
 int fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);ok(fd>=0,"bootstrap-root");
 try{std::istringstream parts(path.substr(1));std::string p;while(std::getline(parts,p,'/')){require(!p.empty()&&p!="."&&p!="..","bootstrap-path",2);int next=openat(fd,p.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);ok(next>=0,"bootstrap-directory");close(fd);fd=next;}return fd;}catch(...){close(fd);throw;}
}
static bool present(int dir,const std::string&name){struct stat st;if(!fstatat(dir,name.c_str(),&st,AT_SYMLINK_NOFOLLOW))return true;ok(errno==ENOENT,"bootstrap-presence");return false;}
static int control(bool create){
 FD parent(directory("/data/local/tmp"));if(create&&mkdirat(parent.n,"x2d-bootstrap",0700))ok(errno==EEXIST,"bootstrap-control-create");
 int fd=openat(parent.n,"x2d-bootstrap",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);ok(fd>=0,"bootstrap-control-open");
 struct stat st;if(fstat(fd,&st)||st.st_uid!=0||(st.st_mode&07777)!=0700){int cause=errno;close(fd);throw Error("bootstrap-control-identity",3,false,cause);}return fd;
}
static struct stat identity(int dir,const std::string&name){struct stat st;ok(!fstatat(dir,name.c_str(),&st,AT_SYMLINK_NOFOLLOW),"bootstrap-identity");return st;}
static bool same(const struct stat&a,const struct stat&b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino;}
static void owned(int dir,const std::string&name,const struct stat&id){require(same(identity(dir,name),id),"bootstrap-owned-object-replaced",90);}
static std::string digest(int dir,const std::string&name,struct stat*id=nullptr){
 FD fd(openat(dir,name.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));ok(fd.n>=0,"bootstrap-hash-open");struct stat before,after;ok(!fstat(fd.n,&before),"bootstrap-hash-stat");require(S_ISREG(before.st_mode)&&before.st_nlink==1&&before.st_uid==0&&(before.st_mode&07777)==0755,"bootstrap-executable-type");
 ReceiptSha h;rsha_init(&h);unsigned char b[65536];for(;;){cancelled();ssize_t n=::read(fd.n,b,sizeof b);if(n<0&&errno==EINTR)continue;ok(n>=0,"bootstrap-hash-read");if(!n)break;rsha_update(&h,b,unsigned(n));}
 ok(!fstat(fd.n,&after),"bootstrap-hash-restat");require(same(before,after)&&after.st_nlink==1&&before.st_size==after.st_size&&before.st_mtim.tv_sec==after.st_mtim.tv_sec&&before.st_mtim.tv_nsec==after.st_mtim.tv_nsec&&before.st_ctim.tv_sec==after.st_ctim.tv_sec&&before.st_ctim.tv_nsec==after.st_ctim.tv_nsec&&same(after,identity(dir,name)),"bootstrap-hash-changed");
 unsigned char raw[32];rsha_final(&h,raw);std::string out;for(auto v:raw){out+="0123456789abcdef"[v>>4];out+="0123456789abcdef"[v&15];}if(id)*id=before;return out;
}
struct Mount {unsigned long flags=0;bool ro=false;};
static Mount mount_state(){
 std::istringstream lines(read("/proc/self/mounts",1024*1024));std::string line;unsigned count=0;Mount result;
 while(std::getline(lines,line)){std::istringstream row(line);std::string a,b,c,d;require(bool(row>>a>>b>>c>>d),"bootstrap-mount-record");if(b!="/system")continue;++count;std::istringstream opts(d);std::string o;bool seen=false;
  while(std::getline(opts,o,',')){if(o=="ro"||o=="rw"){require(!seen,"bootstrap-mount-duplicate");seen=true;result.ro=o=="ro";}else if(o=="nosuid")result.flags|=MS_NOSUID;else if(o=="nodev")result.flags|=MS_NODEV;else if(o=="noexec")result.flags|=MS_NOEXEC;else if(o=="sync")result.flags|=MS_SYNCHRONOUS;else if(o=="dirsync")result.flags|=MS_DIRSYNC;else if(o=="noatime")result.flags|=MS_NOATIME;else if(o=="nodiratime")result.flags|=MS_NODIRATIME;else if(o=="relatime")result.flags|=MS_RELATIME;else if(o=="strictatime")result.flags|=MS_STRICTATIME;}
  require(seen,"bootstrap-mount-mode");
 }require(count==1,"bootstrap-mount-count");return result;
}
static void remount(bool ro){auto st=mount_state();ok(!system_remount(st.flags|MS_REMOUNT|(ro?MS_RDONLY:0)),"bootstrap-remount",90);require(mount_state().ro==ro,"bootstrap-remount-unconfirmed",90);}
static void idle(){require(get("camera","exposure_status","int32")=="0"&&get("storage","storage_status","int32")=="0"&&get("error","active_error","boolean")=="false"&&get("system","system_state","int32")=="1","bootstrap-camera-not-idle");}
static void unmapped(const struct stat&id,bool permit_self=false){
 DIR*proc=opendir("/proc");ok(proc!=nullptr,"bootstrap-proc");
 try{for(;;){errno=0;auto*entry=readdir(proc);if(!entry){ok(errno==0,"bootstrap-proc-next");break;}std::string pid=entry->d_name;if(!digits(pid)||(permit_self&&pid==std::to_string(getpid())))continue;std::string path="/proc/"+pid;FILE*f=fopen((path+"/maps").c_str(),"r");if(!f){if(errno==ENOENT)continue;throw Error("bootstrap-maps-unavailable",1,false,errno);}char*line=nullptr;size_t cap=0;bool matched=false,malformed=false;while(getline(&line,&cap,f)>=0){unsigned major_id,minor_id;unsigned long long inode;if(sscanf(line,"%*s %*s %*s %x:%x %llu",&major_id,&minor_id,&inode)!=3){malformed=true;break;}if(inode==id.st_ino&&major_id==major(id.st_dev)&&minor_id==minor(id.st_dev)){matched=true;break;}}bool failure=ferror(f);free(line);fclose(f);require(!failure&&!malformed,"bootstrap-maps-read");require(!matched,"bootstrap-old-program-active",72);}
 }catch(...){closedir(proc);throw;}closedir(proc);
}
static void put(int dir,const std::string&name,const std::string&text){
 const auto next=name+".next";FD fd(openat(dir,next.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600));ok(fd.n>=0,"bootstrap-receipt-create",90);size_t offset=0;while(offset<text.size()){ssize_t n=write(fd.n,text.data()+offset,text.size()-offset);if(n<0&&errno==EINTR)continue;ok(n>0,"bootstrap-receipt-write",90);offset+=size_t(n);}ok(!fsync(fd.n),"bootstrap-receipt-sync",90);ok(!syscall(SYS_renameat2,dir,next.c_str(),dir,name.c_str(),1),"bootstrap-receipt-rename",90);ok(!fsync(dir),"bootstrap-receipt-parent",90);
}
/* ELF 自删除后必须 exec 释放该 inode，才能 remount RO。固定原厂OS原语，无落盘脚本。 */
static const char*finalize_primitive=
 "b=/system/bin/toybox;r=$7;umask 077;set -C;trap '' 1 2 15;"
 "$b flock -n \"$6\"||r=90;$b mount -oremount,ro /system||r=90;"
 "if test $r = 0;then q=$($b stat -c %d:%i:%u:%f /tmp/x2d-multishot-active);"
 "test \"$q\" = \"$1\" && $b rmdir /tmp/x2d-multishot-active||r=90;fi;"
 "if test $r = 0;then $b printf %s \"$3\" >\"$2\"||exit 90;h=$8;else $b printf %s \"$4\" >\"$2\"||exit 90;h=$9;fi;"
 "$b sync||exit 90;$b printf %s \"$h\" >\"$2.seal\"||exit 90;$b sync||exit 90;"
 "if test $r = 0;then exit \"$5\";else exit 90;fi";
static void finish_by_factory(TransactionGuard&guard,int stage,const std::string&attempt,const std::string&success,const std::string&failure,int primary,const struct stat&task){
 bool preparation_failed=false;try{put(stage,"bootstrap.cleanup.handoff."+attempt,"NATIVE_BOOTSTRAP_RO_HANDOFF_PENDING_V1\n"+success);}catch(const std::exception&e){preparation_failed=true;fprintf(stderr,"BOOTSTRAP_CLEANUP_HANDOFF_RECEIPT_FAILED %s\n",e.what());}
 char mode[32];snprintf(mode,sizeof mode,"%x",unsigned(task.st_mode));
 const auto expected=std::to_string(task.st_dev)+":"+std::to_string(task.st_ino)+":"+std::to_string(task.st_uid)+":"+mode;
 const auto final="/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.final."+attempt,rc=std::to_string(primary),lockfd=std::to_string(guard.fd);
 int flags=fcntl(guard.fd,F_GETFD);if(flags<0||fcntl(guard.fd,F_SETFD,flags&~FD_CLOEXEC)!=0){preparation_failed=true;fprintf(stderr,"BOOTSTRAP_CLEANUP_LOCK_HANDOFF_FAILED errno=%d\n",errno);}
 const char*prepared=preparation_failed?"90":"0";
 auto text_hash=[](const std::string&text){ReceiptSha h;rsha_init(&h);rsha_update(&h,reinterpret_cast<const unsigned char*>(text.data()),unsigned(text.size()));unsigned char raw[32];rsha_final(&h,raw);std::string result;for(auto byte:raw){result+="0123456789abcdef"[byte>>4];result+="0123456789abcdef"[byte&15];}return result;};const auto success_hash=text_hash(success),failure_hash=text_hash(failure);
 char*args[]={const_cast<char*>("sh"),const_cast<char*>("-c"),const_cast<char*>(finalize_primitive),const_cast<char*>("x2d-bootstrap-finalize"),const_cast<char*>(expected.c_str()),const_cast<char*>(final.c_str()),const_cast<char*>(success.c_str()),const_cast<char*>(failure.c_str()),const_cast<char*>(rc.c_str()),const_cast<char*>(lockfd.c_str()),const_cast<char*>(prepared),const_cast<char*>(success_hash.c_str()),const_cast<char*>(failure_hash.c_str()),nullptr};
 char path[]="PATH=/system/bin";char lang[]="LC_ALL=C";char*env[]={path,lang,nullptr};execve("/system/bin/sh",args,env);
 int cause=errno;fcntl(guard.fd,F_SETFD,flags);throw Error("bootstrap-cleanup-factory-exec",90,true,cause);
}
static int commit(const std::string&attempt,const std::string&old_hash,const std::string&new_hash){
 require(hex(attempt,16)&&(old_hash=="absent"||hex(old_hash,64))&&hex(new_hash,64)&&old_hash!=new_hash,"bootstrap-arguments",2);
 const std::string seed="x2d-"+new_hash.substr(0,8),target="x2d-native-setup",retained="x2d-native-setup.previous."+attempt;
 TransactionGuard transaction;FD bin(directory("/system/bin")),stage(control(true)),runtime(directory("/tmp"));
 require(!present(stage.n,"bootstrap.claim."+attempt)&&!present(stage.n,"bootstrap.final."+attempt),"bootstrap-already-attempted",3);
 require(mount_state().ro,"bootstrap-system-initially-writable",3);struct stat source{},old{};require(digest(bin.n,seed,&source)==new_hash,"bootstrap-seed-digest");struct stat self;ok(!stat("/proc/self/exe",&self),"bootstrap-self-stat");require(same(self,source)&&executable(getpid())=="/system/bin/"+seed,"bootstrap-must-execute-seed",3);
 const bool existed=old_hash!="absent";if(existed){require(digest(bin.n,target,&old)==old_hash,"bootstrap-old-digest");unmapped(old);}else require(!present(bin.n,target),"bootstrap-unknown-existing-target",3);require(!present(bin.n,retained),"bootstrap-retained-conflict",3);idle();
 put(stage.n,"bootstrap.claim."+attempt,"NATIVE_BOOTSTRAP_CLAIM_V1\nattempt="+attempt+"\nold="+old_hash+"\nnew="+new_hash+"\n");
 int primary=0,restore=0,cause=0;std::string step="none";bool task=false,task_known=false,changed=false,switched=false,retained_ready=false,completed=false;struct stat task_id{};
 try{
  ok(!mkdirat(runtime.n,"x2d-multishot-active",0700),"bootstrap-task-lock",72);task=true;task_id=identity(runtime.n,"x2d-multishot-active");task_known=true;idle();if(existed)unmapped(old);owned(bin.n,seed,source);if(existed)owned(bin.n,target,old);else require(!present(bin.n,target),"bootstrap-target-race",3);cancelled();changed=true;remount(false);
  ok(!syscall(SYS_renameat2,bin.n,seed.c_str(),bin.n,target.c_str(),existed?2:1),"bootstrap-switch");switched=true;owned(bin.n,target,source);
  if(existed){owned(bin.n,seed,old);ok(!syscall(SYS_renameat2,bin.n,seed.c_str(),bin.n,retained.c_str(),1),"bootstrap-retain-old");retained_ready=true;}ok(!fsync(bin.n),"bootstrap-switch-sync");require(digest(bin.n,target)==new_hash,"bootstrap-target-digest");cancelled();remount(true);completed=true;
 }catch(const Error&e){primary=e.rc;restore=e.recovery_failed?90:0;cause=e.system_errno;step=e.what();}catch(const std::exception&e){primary=1;step="native-exception";fprintf(stderr,"BOOTSTRAP_EXCEPTION %s\n",e.what());}
 {Recovery recovery;
  if(!primary&&cn_signal_pending()){primary=143;step="cancelled";}
  if(primary&&switched){try{remount(false);owned(bin.n,target,source);if(existed){const auto&location=retained_ready?retained:seed;owned(bin.n,location,old);ok(!syscall(SYS_renameat2,bin.n,location.c_str(),bin.n,target.c_str(),2),"bootstrap-rollback-exchange",90);if(retained_ready){owned(bin.n,retained,source);ok(!syscall(SYS_renameat2,bin.n,retained.c_str(),bin.n,seed.c_str(),1),"bootstrap-rollback-seed",90);}require(digest(bin.n,target)==old_hash,"bootstrap-rollback-old-digest",90);}else{require(!present(bin.n,seed),"bootstrap-rollback-seed-conflict",90);ok(!syscall(SYS_renameat2,bin.n,target.c_str(),bin.n,seed.c_str(),1),"bootstrap-rollback-new",90);}ok(!fsync(bin.n),"bootstrap-rollback-sync",90);switched=false;completed=false;}catch(const std::exception&e){restore=90;fprintf(stderr,"BOOTSTRAP_ROLLBACK_UNCONFIRMED %s\n",e.what());}}
  if(changed)try{remount(true);}catch(const std::exception&e){restore=90;fprintf(stderr,"BOOTSTRAP_READONLY_UNCONFIRMED %s\n",e.what());}
  if(task&&!restore)try{require(task_known,"bootstrap-task-unknown",90);owned(runtime.n,"x2d-multishot-active",task_id);ok(!unlinkat(runtime.n,"x2d-multishot-active",AT_REMOVEDIR),"bootstrap-task-unlock",90);}catch(const std::exception&e){restore=90;fprintf(stderr,"BOOTSTRAP_UNLOCK_UNCONFIRMED %s\n",e.what());}
  try{put(stage.n,"bootstrap.final."+attempt,"NATIVE_BOOTSTRAP_V1\nattempt="+attempt+"\nold="+old_hash+"\nnew="+new_hash+"\nprimary_step="+step+"\nprimary_errno="+std::to_string(cause)+"\nprimary_rc="+std::to_string(primary)+"\nrestore_rc="+std::to_string(restore)+"\nfinal_rc="+std::to_string(restore?restore:primary)+"\nsetup_ready="+(completed?"1":"0")+"\nretained="+(completed&&existed?retained:"none")+"\n");}catch(const std::exception&e){restore=90;fprintf(stderr,"BOOTSTRAP_FINAL_UNCONFIRMED %s\n",e.what());}
 }return restore?restore:primary;
}
/* 只收回本次部署可证明的工具；未知历史 helper 保留并拒绝“清理完成”。 */
static int cleanup(const std::string&attempt,const std::string&new_hash,const std::string&old_hash,const std::string&previous_attempt){
 require(hex(attempt,16)&&hex(new_hash,64)&&(old_hash=="absent"||hex(old_hash,64))&&
 ((previous_attempt=="none"&&old_hash=="absent")||hex(previous_attempt,16)),"bootstrap-cleanup-arguments",2);
 TransactionGuard transaction;FD bin(directory("/system/bin")),stage(control(true)),runtime(directory("/tmp"));
 require(mount_state().ro,"bootstrap-cleanup-initially-writable",3);
 require(!present(stage.n,"bootstrap.cleanup.claim."+attempt)&&!present(stage.n,"bootstrap.cleanup.final."+attempt),"bootstrap-cleanup-already-attempted",3);
 const std::string target="x2d-native-setup",previous="x2d-native-setup.previous."+previous_attempt;
 require(digest(bin.n,"sh")=="528507fdee38348abd5805016592e489925de84b3fec3b6fadee157e6c940f44"&&digest(bin.n,"toybox")=="999a0669ef654efbda54bc585c0f3c00643d1af61c985bfb7968cfd2f9aed840","bootstrap-cleanup-factory-tools",3);
 struct Item{std::string name,hash;struct stat id;bool self;};std::vector<Item> items;
 auto add=[&](const std::string&name,const std::string&hash,bool self){struct stat id{};require(digest(bin.n,name,&id)==hash,"bootstrap-cleanup-digest",3);unmapped(id,self);items.push_back({name,hash,id,self});};
 struct stat self{};ok(!stat("/proc/self/exe",&self),"bootstrap-cleanup-self-stat");
 require(executable(getpid())=="/system/bin/"+target&&same(self,identity(bin.n,target)),"bootstrap-cleanup-not-current-setup",3);
 if(old_hash!="absent"&&present(bin.n,previous))add(previous,old_hash,false);
 for(const auto&hash:{new_hash,old_hash})if(hash!="absent"){auto name="x2d-"+hash.substr(0,8);if(present(bin.n,name))add(name,hash,false);}
 // 已知的 /var/run 兼容性失败版本：可能只落地 seed，未完成 commit。
 // 仍要求完整摘要、普通文件身份和未映射验证；不接受任意同名前缀。
 for(const std::string hash:{"a1815fcaec5200734d94f764a42f7320bd6ee8094767c4d19bf97f3c61863dfa"}){
  auto name="x2d-"+hash.substr(0,8);bool known=false;for(const auto&item:items)if(item.name==name)known=true;
  if(!known&&present(bin.n,name))add(name,hash,false);
 }
 add(target,new_hash,true); // 自身最后删除；之前的错误仍可保留可诊断的当前工具。
 FD scanfd(openat(bin.n,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ok(scanfd.n>=0,"bootstrap-cleanup-scan-open");DIR*scan=fdopendir(dup(scanfd.n));ok(scan!=nullptr,"bootstrap-cleanup-scan");
 try{for(;;){errno=0;auto*entry=readdir(scan);if(!entry){ok(errno==0,"bootstrap-cleanup-scan-read");break;}std::string name=entry->d_name;
   if(name.rfind("x2d-native-setup.previous.",0)!=0&&!(name.size()==12&&name.rfind("x2d-",0)==0&&hex(name.substr(4),8)))continue;
   bool known=false;for(const auto&item:items)if(item.name==name)known=true;require(known,"bootstrap-cleanup-unknown-helper",3);
  }}catch(...){closedir(scan);throw;}closedir(scan);idle();
 put(stage.n,"bootstrap.cleanup.claim."+attempt,"NATIVE_BOOTSTRAP_CLEANUP_CLAIM_V1\nattempt="+attempt+"\nnew="+new_hash+"\nold="+old_hash+"\nprevious="+previous_attempt+"\n");
 int primary=0,restore=0,cause=0;std::string step="none";size_t removed=0;bool task=false,task_known=false,changed=false;struct stat task_id{};
 try{ok(!mkdirat(runtime.n,"x2d-multishot-active",0700),"bootstrap-cleanup-task-lock",72);task=true;task_id=identity(runtime.n,"x2d-multishot-active");task_known=true;idle();
  for(const auto&item:items){owned(bin.n,item.name,item.id);require(digest(bin.n,item.name)==item.hash,"bootstrap-cleanup-content-changed");unmapped(item.id,item.self);}
  cancelled();changed=true;remount(false);
  for(const auto&item:items){cancelled();owned(bin.n,item.name,item.id);unmapped(item.id,item.self);ok(!unlinkat(bin.n,item.name.c_str(),0),"bootstrap-cleanup-unlink");++removed;}
  ok(!fsync(bin.n),"bootstrap-cleanup-sync",90);
 }catch(const Error&e){primary=e.rc;cause=e.system_errno;restore=e.recovery_failed?90:0;step=e.what();}catch(const std::exception&e){primary=1;step="native-exception";fprintf(stderr,"BOOTSTRAP_CLEANUP_EXCEPTION %s\n",e.what());}
 {Recovery recovery;
  if(!primary&&cn_signal_pending()){primary=143;step="cancelled";}
  if(removed==items.size()){
   const auto base="NATIVE_BOOTSTRAP_CLEANUP_V1\nattempt="+attempt+"\nnew="+new_hash+"\nold="+old_hash+"\nprevious="+previous_attempt+"\nprimary_step="+step+"\nprimary_errno="+std::to_string(cause)+"\nprimary_rc="+std::to_string(primary)+"\n";
   const auto tail="removed="+std::to_string(removed)+"\nexpected="+std::to_string(items.size())+"\ntools_removed=1\n";
   try{require(task&&task_known,"bootstrap-cleanup-task-handoff",90);finish_by_factory(transaction,stage.n,attempt,base+"restore_rc=0\nfinal_rc="+std::to_string(primary)+"\n"+tail,base+"restore_rc=90\nfinal_rc=90\n"+tail,primary,task_id);}catch(const std::exception&e){fprintf(stderr,"BOOTSTRAP_CLEANUP_HANDOFF_UNCONFIRMED %s\n",e.what());return 90;}
  }
  if(changed)try{remount(true);}catch(const std::exception&e){restore=90;fprintf(stderr,"BOOTSTRAP_CLEANUP_RO_UNCONFIRMED %s\n",e.what());}
  if(task&&!restore)try{require(task_known,"bootstrap-cleanup-task-unknown",90);owned(runtime.n,"x2d-multishot-active",task_id);ok(!unlinkat(runtime.n,"x2d-multishot-active",AT_REMOVEDIR),"bootstrap-cleanup-task-unlock",90);}catch(const std::exception&e){restore=90;fprintf(stderr,"BOOTSTRAP_CLEANUP_UNLOCK_UNCONFIRMED %s\n",e.what());}
  try{put(stage.n,"bootstrap.cleanup.final."+attempt,"NATIVE_BOOTSTRAP_CLEANUP_V1\nattempt="+attempt+"\nnew="+new_hash+"\nold="+old_hash+"\nprevious="+previous_attempt+"\nprimary_step="+step+"\nprimary_errno="+std::to_string(cause)+"\nprimary_rc="+std::to_string(primary)+"\nrestore_rc="+std::to_string(restore)+"\nfinal_rc="+std::to_string(restore?restore:primary)+"\nremoved="+std::to_string(removed)+"\nexpected="+std::to_string(items.size())+"\ntools_removed="+(removed==items.size()?"1":"0")+"\n");put(stage.n,"bootstrap.cleanup.final."+attempt+".seal",sha256("/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.final."+attempt));}catch(const std::exception&e){restore=90;fprintf(stderr,"BOOTSTRAP_CLEANUP_FINAL_UNCONFIRMED %s\n",e.what());}
 }return restore?restore:primary;
}
static std::string b64(const std::string&s){const char*t="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;for(size_t i=0;i<s.size();i+=3){unsigned a=static_cast<unsigned char>(s[i]),b=i+1<s.size()?static_cast<unsigned char>(s[i+1]):0,c=i+2<s.size()?static_cast<unsigned char>(s[i+2]):0;out+=t[a>>2];out+=t[((a&3)<<4)|(b>>4)];out+=i+1<s.size()?t[((b&15)<<2)|(c>>6)]:'=';out+=i+2<s.size()?t[c&63]:'=';}return out;}
static std::string collect(const std::string&attempt){
 require(hex(attempt,16),"bootstrap-read-attempt",2);FD stage(control(false));std::string name="bootstrap.final."+attempt;if(!present(stage.n,name))return "{\"protocol\":\"NATIVE_BOOTSTRAP_READ_V1\",\"attempt\":\""+attempt+"\",\"state\":\"ABSENT\",\"data\":\"\"}\n";
 FD fd(openat(stage.n,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));ok(fd.n>=0,"bootstrap-read-open");struct stat before,after;ok(!fstat(fd.n,&before),"bootstrap-read-stat");require(S_ISREG(before.st_mode)&&before.st_nlink==1&&before.st_size>=0&&before.st_size<=16384,"bootstrap-read-type");std::string text;char b[1024];for(;;){ssize_t n=::read(fd.n,b,sizeof b);if(n<0&&errno==EINTR)continue;ok(n>=0,"bootstrap-read-content");if(!n)break;require(text.size()+size_t(n)<=16384,"bootstrap-read-size");text.append(b,size_t(n));}ok(!fstat(fd.n,&after),"bootstrap-read-restat");require(same(before,after)&&same(before,identity(stage.n,name))&&after.st_nlink==1&&before.st_size==after.st_size&&text.size()==size_t(after.st_size)&&before.st_mtim.tv_sec==after.st_mtim.tv_sec&&before.st_mtim.tv_nsec==after.st_mtim.tv_nsec,"bootstrap-read-unstable");return "{\"protocol\":\"NATIVE_BOOTSTRAP_READ_V1\",\"attempt\":\""+attempt+"\",\"state\":\"PRESENT\",\"data\":\""+b64(text)+"\"}\n";
}
} }
extern "C" int cn_bootstrap_main(int argc,char**argv){try{cn::require(argc>=2,"bootstrap-command",2);std::string op=argv[1];if(op=="bootstrap-commit"){cn::require(argc==5,"bootstrap-arguments",2);return cn::bootstrap::commit(argv[2],argv[3],argv[4]);}if(op=="bootstrap-cleanup"){cn::require(argc==6,"bootstrap-cleanup-arguments",2);return cn::bootstrap::cleanup(argv[2],argv[3],argv[4],argv[5]);}if(op=="bootstrap-read"){cn::require(argc==3,"bootstrap-read-arguments",2);auto text=cn::bootstrap::collect(argv[2]);if(fwrite(text.data(),1,text.size(),stdout)!=text.size()||fflush(stdout))return 1;return 0;}throw cn::Error("bootstrap-command",2);}catch(const cn::Error&e){fprintf(stderr,"BOOTSTRAP_REFUSED %s rc=%d errno=%d\n",e.what(),e.rc,e.system_errno);return e.recovery_failed?90:e.rc;}catch(const std::exception&e){fprintf(stderr,"BOOTSTRAP_REFUSED %s\n",e.what());return 1;}}
