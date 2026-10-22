#include "native_ui_ready.hpp"
#include "native_common.hpp"
#include "ui_receipt.h"
#include <sys/random.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <limits>
namespace cn {
namespace {
const char*module="/system/lib64/libx2d_native_menu.so";
struct Fd {int value;explicit Fd(int v):value(v){}~Fd(){if(value>=0)::close(value);}Fd(const Fd&)=delete;};
bool owned(const struct stat&s){return S_ISREG(s.st_mode)&&s.st_nlink==1&&s.st_uid==geteuid()&&!(s.st_mode&0022);}
bool same(const struct stat&a,const struct stat&b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_size==b.st_size&&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;}
bool digest(const char*s){if(!s||strnlen(s,65)!=64)return false;for(unsigned i=0;i<64;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;}
struct Identity {uint64_t pid,start;bool operator==(const Identity&o)const{return pid==o.pid&&start==o.start;}};
Identity identity(){auto ps=pids("camera-gui");require(ps.size()==1&&ps[0]>0&&executable(ps[0])=="/system/bin/camera-gui","ui-ready-process");auto s=read("/proc/"+std::to_string(ps[0])+"/stat",16384);auto end=s.rfind(')');require(end!=s.npos,"ui-ready-stat");std::istringstream fields(s.substr(end+1));std::string start;for(unsigned i=3;i<=22;i++)require(bool(fields>>start),"ui-ready-start");require(digits(start),"ui-ready-start-format");uint64_t n=0;for(char c:start){unsigned d=unsigned(c-'0');require(n<=(UINT64_MAX-d)/10,"ui-ready-start-overflow");n=n*10+d;}require(n!=0,"ui-ready-start-zero");return {uint64_t(ps[0]),n};}
std::string nonce(){unsigned char raw[16];size_t done=0;unsigned tries=0;while(done<sizeof raw&&++tries<=32){ssize_t n=getrandom(raw+done,sizeof raw-done,0);if(n<0&&errno==EINTR)continue;require(n>0,"ui-ready-random");done+=size_t(n);}require(done==sizeof raw,"ui-ready-random-short");std::string out;for(auto b:raw){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;}
void check_target(int dir,const char*name){struct stat s;if(fstatat(dir,name,&s,AT_SYMLINK_NOFOLLOW)==0)require(owned(s),"ui-ready-file-type");else require(errno==ENOENT,"ui-ready-file-stat");}
void query(int dir,const std::string&n){const auto tmp=".x2d-ui-query-"+n;Fd f(openat(dir,tmp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));require(f.value>=0,"ui-ready-query-create");struct stat owner{};require(!fstat(f.value,&owner)&&owned(owner),"ui-ready-query-owner");try{const auto text="UI_QUERY_R1 "+n+"\n";size_t done=0;unsigned tries=0;while(done<text.size()&&++tries<=32){ssize_t k=::write(f.value,text.data()+done,text.size()-done);if(k<0&&errno==EINTR)continue;require(k>0,"ui-ready-query-write");done+=size_t(k);}require(done==text.size()&&!fsync(f.value),"ui-ready-query-flush");check_target(dir,"x2d-native-ui.query");struct stat current;require(!fstatat(dir,tmp.c_str(),&current,AT_SYMLINK_NOFOLLOW)&&owned(current)&&owner.st_ino==current.st_ino&&owner.st_dev==current.st_dev,"ui-ready-query-replaced");require(!renameat(dir,tmp.c_str(),dir,"x2d-native-ui.query"),"ui-ready-query-publish");}catch(...){struct stat s;if(!fstatat(dir,tmp.c_str(),&s,AT_SYMLINK_NOFOLLOW)&&s.st_dev==owner.st_dev&&s.st_ino==owner.st_ino)unlinkat(dir,tmp.c_str(),0);throw;}}
std::string file(int dir,const char*name,size_t max){Fd f(openat(dir,name,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC));if(f.value<0){require(errno==ENOENT,"ui-ready-read-open");return {};}struct stat a,b;require(!fstat(f.value,&a)&&owned(a)&&a.st_size>=0&&uint64_t(a.st_size)<=max,"ui-ready-read-type");std::string out;char buf[513];for(unsigned i=0;i<32;i++){ssize_t n=::read(f.value,buf,sizeof buf);if(n<0&&errno==EINTR)continue;require(n>=0,"ui-ready-read-data");if(!n){require(!fstat(f.value,&b)&&same(a,b),"ui-ready-read-changed");return out;}require(out.size()+size_t(n)<=max,"ui-ready-read-limit");out.append(buf,size_t(n));}throw Error("ui-ready-read-retries");}
}
void native_ui_ready(const UiReadyContract&contract,unsigned timeout){
 require(digest(contract.module_sha256)&&digest(contract.gui_sha256)&&contract.capabilities&&timeout&&timeout<=30000,"ui-ready-contract");
 require(sha256(module)==contract.module_sha256&&sha256("/system/bin/camera-gui")==contract.gui_sha256,"ui-ready-build");const auto expected=identity();
 Fd dir(open("/tmp",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat ds;require(dir.value>=0&&!fstat(dir.value,&ds)&&S_ISDIR(ds.st_mode)&&(ds.st_uid==0||ds.st_uid==geteuid())&&(!(ds.st_mode&0022)||(ds.st_mode&S_ISVTX)),"ui-ready-tmp");
 const auto begin=monotonic_ms();require(begin&&begin<=UINT64_MAX-timeout,"ui-ready-clock");auto current=nonce();uint64_t requested=begin;query(dir.value,current);
 for(unsigned tries=0;tries<=timeout/50+1;tries++){
  cancelled();const auto now=monotonic_ms();require(now>=begin&&now-begin<=timeout,"ui-ready-timeout");require(identity()==expected,"ui-ready-process-changed");
  auto q=file(dir.value,"x2d-native-ui.query",45);char actual[33];require(x2d_ui_query_parse(q.data(),q.size(),actual)&&current==actual,"ui-ready-request-replaced");
  const auto text=file(dir.value,"x2d-native-ui.status",512);X2dUiReceipt r{};
  if(x2d_ui_receipt_parse(text.data(),text.size(),&r)&&current==r.nonce){
   require(r.pid==expected.pid&&r.start_ticks==expected.start&&contract.module_sha256==std::string(r.build)&&contract.gui_sha256==std::string(r.gui),"ui-ready-reply-identity");
   if(x2d_ui_receipt_ready(&r,current.c_str(),expected.pid,expected.start,contract.module_sha256,contract.gui_sha256,contract.capabilities,requested,now)){
    require(sha256(module)==contract.module_sha256&&sha256("/system/bin/camera-gui")==contract.gui_sha256&&identity()==expected,"ui-ready-final-identity");
    auto latest=file(dir.value,"x2d-native-ui.query",45);char finalNonce[33];require(x2d_ui_query_parse(latest.data(),latest.size(),finalNonce)&&current==finalNonce,"ui-ready-final-request");
    const auto finalTime=monotonic_ms();require(finalTime>=begin&&finalTime-begin<=timeout,"ui-ready-timeout");
    if(x2d_ui_receipt_ready(&r,current.c_str(),expected.pid,expected.start,contract.module_sha256,contract.gui_sha256,contract.capabilities,requested,finalTime))return;
   }
   require(r.state!=X2D_UI_FAILED&&r.state!=X2D_UI_STOPPED,"ui-ready-negative-reply");
   // Poll readiness with a new challenge; never reuse a consumed reply.
   current=nonce();requested=now;query(dir.value,current);
  }
  sleep_ms(50);
 }
 throw Error("ui-ready-timeout");
}
}
