#include "native_common.hpp"
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
static pid_t denied=0;static int denied_error=EACCES;static unsigned checks=0;
static ssize_t test_readlink(const char*p,char*b,size_t n){
 char expected[64];snprintf(expected,sizeof expected,"/proc/%d/exe",int(denied));
 if(denied&&strcmp(p,expected)==0){errno=denied_error;return -1;}
 return readlink(p,b,n);
}
extern "C" int cn_cancel_requested(){return 0;}
extern "C" void cn_recovery_mode(int){}
extern "C" int cn_job_valid(const char*){return 0;}
#define readlink test_readlink
#include "native_common.cpp"
#undef readlink
static void check(bool ok,const char*message){++checks;if(!ok){printf("PROC_CHECK_FAIL %s\n",message);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}
static pid_t child(const char*name){
 int ready[2];check(pipe(ready)==0,"pipe");pid_t p=fork();check(p>=0,"fork");
 if(!p){close(ready[0]);prctl(PR_SET_NAME,name);char c='r';write(ready[1],&c,1);close(ready[1]);for(;;)pause();}
 close(ready[1]);char c=0;check(read(ready[0],&c,1)==1&&c=='r',"child-ready");close(ready[0]);return p;
}
static void stop(pid_t p){check(kill(p,SIGTERM)==0,"kill-owned-child");check(waitpid(p,nullptr,0)==p,"reap-owned-child");}
static void fails(const char*name){bool failed=false;try{cn::pids(name);}catch(const cn::Error&e){failed=std::string(e.what())=="proc-executable-unconfirmed"&&e.system_errno==denied_error;}check(failed,"matching or unknown error denied");}
int main(){
 check(mount("proc","/proc","proc",0,nullptr)==0,"mount-proc");prctl(PR_SET_NAME,"init");
 denied=child("unrelated");
 for(int e:{EACCES,EPERM,ENOENT}){denied_error=e;auto p=cn::pids("init");check(std::find(p.begin(),p.end(),getpid())!=p.end(),"parent found despite unrelated inaccessible process");check(std::find(p.begin(),p.end(),denied)==p.end(),"unrelated inaccessible excluded");}
 denied_error=EIO;fails("init");stop(denied);
 denied=child("init");for(int e:{EACCES,EPERM,ENOENT}){denied_error=e;fails("init");}stop(denied);
 denied=child("camera-service-");denied_error=EACCES;fails("camera-service-long-name");stop(denied);
 denied=0;auto p=child("different-comm");auto found=cn::pids("init");check(std::find(found.begin(),found.end(),p)!=found.end(),"readable exe still identifies renamed comm");stop(p);
 printf("PROCESS_READINESS_PASS checks=%u camera_requests=0\n",checks);fflush(stdout);reboot(RB_POWER_OFF);return 0;
}
