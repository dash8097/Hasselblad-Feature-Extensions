/* 编译原卸载函数，隔离 Linux 中验证进程身份与正常退出；不访问相机。 */
#include "native_common.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstdarg>
static int test_open(const char*,int,...);
namespace cn {std::vector<pid_t> test_pids(const std::string&);}
#define open test_open
#define pids test_pids
#include "native_uninstall.cpp"
#undef pids
#undef open
#include <sys/prctl.h>
#include <sys/reboot.h>
#include <cstring>
extern "C" int cn_cancel_requested(void){return 0;}
extern "C" int cn_signal_pending(void){return 0;}
extern "C" void cn_recovery_mode(int){}
static unsigned checks;
static bool stale_enumeration;static pid_t stale_pid,deny_pid;static int denied_errno;
static int test_open(const char*path,int flags,...){
 mode_t mode=0;if(flags&O_CREAT){va_list args;va_start(args,flags);mode=va_arg(args,int);va_end(args);}
 if(deny_pid&&std::string(path)=="/proc/"+std::to_string(deny_pid)+"/stat"){errno=denied_errno;return -1;}
 return ::open(path,flags,mode);
}
namespace cn {std::vector<pid_t> test_pids(const std::string&name){if(stale_enumeration&&name=="x2d-esflash")return {stale_pid};return pids(name);}}
static void check(bool v){++checks;if(!v){printf("FLASH_UNINSTALL_FAILED check=%u\n",checks);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}
static void term(int){_exit(0);}
static pid_t launch(const char*path){
 unlink("/ready");pid_t p=fork();check(p>=0);if(!p){execl(path,path,"hold",nullptr);_exit(127);}
 for(unsigned i=0;i<100&&!cn::exists("/ready");++i)usleep(10000);check(cn::exists("/ready"));return p;
}
static void wait_ok(pid_t p){int status;check(waitpid(p,&status,0)==p&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}
int main(int argc,char**){
 if(argc>1){signal(SIGTERM,term);prctl(PR_SET_NAME,"x2d-esflash");int fd=open("/ready",O_CREAT|O_WRONLY,0600);close(fd);for(;;)pause();}
 setvbuf(stdout,nullptr,_IONBF,0);check(!mount("proc","/proc","proc",0,nullptr));
 cn::setup::LegacyTarget target={"u169","/system/bin/x2d-esflash","ABSENT","",0755};
 cn::uninstall::Entry entry{};entry.target=&target;entry.before=cn::sha256(target.path);
 std::vector<cn::uninstall::Entry> entries={entry};
 pid_t p=launch(target.path);cn::uninstall::stop_esflash(entries);wait_ok(p);
 // Enumeration has already completed when the last process is reaped. The
 // real /proc read now returns ENOENT; the former strict zombie() read failed.
 stale_pid=p;bool baseline_failed=false;try{cn::uninstall::zombie(p);}catch(const cn::Error&e){baseline_failed=std::string(e.what())=="read-open:/proc/"+std::to_string(p)+"/stat";}check(baseline_failed);puts("FLASH_UNINSTALL_STALE_BASELINE_REJECTED");
 stale_enumeration=true;bool accepted=false;
 try{cn::uninstall::stop_esflash(entries);accepted=true;}catch(const cn::Error&){}check(accepted);stale_enumeration=false;
 // A normal worker/guardian pair may disappear independently during shutdown.
 p=launch(target.path);pid_t guardian=launch(target.path);cn::uninstall::stop_esflash(entries);wait_ok(p);wait_ok(guardian);
 // A denial is never treated as a disappearing process and no signal is sent.
 for(int cause:{EACCES,EPERM}){
  p=launch(target.path);deny_pid=p;denied_errno=cause;bool denied=false;
  try{cn::uninstall::stop_esflash(entries);}catch(const cn::Error&e){denied=std::string(e.what())=="uninstall-flash-stat-open"&&e.system_errno==cause;}
  deny_pid=0;check(denied&&!kill(p,0));kill(p,SIGTERM);wait_ok(p);
 }
 p=launch(target.path);bool refused=false;try{cn::uninstall::stop_esflash({});}catch(const cn::Error&e){refused=std::string(e.what())=="uninstall-flash-unowned";}check(refused&&!kill(p,0));kill(p,SIGTERM);wait_ok(p);
 p=launch(target.path);entries[0].before=std::string(64,'0');refused=false;try{cn::uninstall::stop_esflash(entries);}catch(const cn::Error&e){refused=std::string(e.what())=="uninstall-flash-file-changed";}check(refused&&!kill(p,0));kill(p,SIGTERM);wait_ok(p);
 entries[0].before=entry.before;p=launch("/foreign/x2d-esflash");refused=false;try{cn::uninstall::stop_esflash(entries);}catch(const cn::Error&e){refused=std::string(e.what())=="uninstall-flash-executable";}check(refused&&!kill(p,0));kill(p,SIGTERM);wait_ok(p);
 printf("FLASH_UNINSTALL_PASSED cases=8 assertions=%u\n",checks);reboot(RB_POWER_OFF);return 0;
}
