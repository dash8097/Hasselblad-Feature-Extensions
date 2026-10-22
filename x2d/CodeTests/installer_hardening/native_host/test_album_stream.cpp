#include "native_common.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
static unsigned checks=0;
static uint64_t cancel_at=0;
extern "C" int cn_cancel_requested(){return cancel_at&&cn::monotonic_ms()>=cancel_at;}
extern "C" void cn_recovery_mode(int){}
extern "C" int cn_job_valid(const char*){return 0;}
static void check(bool ok){++checks;if(!ok)throw cn::Error("test-failed");}
static void emit(const std::string&s){size_t p=0;while(p<s.size()){auto n=write(1,s.data()+p,s.size()-p);if(n<=0)_exit(98);p+=size_t(n);}}
int main(int,char**argv){
 if(strstr(argv[0],"dbus-send")){
  auto mode=cn::value("/case");size_t offset=std::stoul(cn::value("/offset"));
  std::string chunk(4096,'x');
  while(offset){size_t n=offset<chunk.size()?offset:chunk.size();emit(chunk.substr(0,n));offset-=n;}
  emit(mode=="absent"?"string \"B12345670\"":"string \"B1234567\"");
  if(mode=="timeout"||mode=="cancel"){sleep(30);return 0;}
  if(mode=="budget"){for(unsigned i=0;i<32769;i++)emit(chunk);}
  emit("\nEND\n");return mode=="nonzero"?7:0;
 }
 setvbuf(stdout,nullptr,_IONBF,0);mkdir("/bin",0755);symlink("/init","/bin/dbus-send");
 try{
  auto configure=[](const char*m,size_t offset){cn::write_replace("/case",m);cn::write_replace("/offset",std::to_string(offset));};
  for(size_t n: {size_t(0),size_t(2097152),size_t(4194304)}){
   configure("present",n);auto r=cn::album_contains("100HASBL","B1234567");check(!r.rc&&r.output.rfind("matched=1\n",0)==0&&r.output.size()<80);
  }
  for(size_t n=4080;n<4097;n++){
   configure("present",n);auto r=cn::album_contains("100HASBL","B1234567");check(!r.rc&&r.output.rfind("matched=1\n",0)==0);
  }
  configure("absent",2097152);check(cn::album_contains("100HASBL","B1234567").output.rfind("matched=0\n",0)==0);
  configure("nonzero",2097152);check(cn::album_contains("100HASBL","B1234567").rc==7);
  configure("timeout",0);try{cn::album_contains("100HASBL","B1234567");check(false);}catch(const cn::Error&e){check(e.rc==124);}
  configure("cancel",0);cancel_at=cn::monotonic_ms()+100;try{cn::album_contains("100HASBL","B1234567");check(false);}catch(const cn::Error&e){check(e.rc==143);}cancel_at=0;
  configure("budget",0);try{cn::album_contains("100HASBL","B1234567");check(false);}catch(const cn::Error&e){check(std::string(e.what())=="album-output-limit");}
  configure("present",2097152);try{cn::command({"/bin/dbus-send"});check(false);}catch(const cn::Error&e){check(std::string(e.what())=="command-output-limit");}
  int status;check(waitpid(-1,&status,WNOHANG)==-1&&errno==ECHILD);
  printf("ALBUM_STREAM_PASS checks=%u\n",checks);
 }catch(const std::exception&e){printf("ALBUM_STREAM_FAIL checks=%u reason=%s\n",checks,e.what());}
 sync();reboot(RB_POWER_OFF);return 0;
}
