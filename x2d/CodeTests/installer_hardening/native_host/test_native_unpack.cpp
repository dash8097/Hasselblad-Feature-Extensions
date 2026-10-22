#include <unistd.h>
#include <sys/syscall.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
static bool deny_commit=false,deny_cleanup=false,swap_directory=false;
static long test_rename(long number,int a,const char*b,int c,const char*d,int flags){if(deny_commit&&!strcmp(d,"payload")){errno=EIO;return -1;}if(swap_directory&&!strcmp(d,"payload")){swap_directory=false;if(syscall(number,a,b,a,"saved-verified",1)||mkdirat(a,b,0700))return -1;int fd=openat(a,b,O_DIRECTORY|O_RDONLY),file=fd<0?-1:openat(fd,"foreign-marker",O_CREAT|O_EXCL|O_WRONLY,0600);if(file<0)return -1;write(file,"foreign",7);close(file);close(fd);}return syscall(number,a,b,c,d,flags);}
static int test_unlink(int a,const char*b,int flags){if(deny_cleanup&&!flags){errno=EACCES;return -1;}return unlinkat(a,b,flags);}
#define syscall test_rename
#define unlinkat test_unlink
#include "native_unpack.cpp"
#undef syscall
#undef unlinkat
#include "unpack_cases.hpp"
#include <sys/mount.h>
#include <sys/reboot.h>
static unsigned checks=0;
static int stop_requested=0,recovery_depth=0;
extern "C" int cn_cancel_requested(){return recovery_depth?0:stop_requested;}
extern "C" int cn_signal_pending(){return stop_requested;}
extern "C" void cn_recovery_mode(int on){recovery_depth+=on?1:-1;}
extern "C" int cn_job_valid(const char*){return 0;}
static void check(bool value,const char*label){checks++;if(!value)throw std::runtime_error(label);}
static void make(const char*p){check(!mkdir(p,0700)||errno==EEXIST,"mkdir-fixture");}
static void extract_cases(){
 using namespace cn::unpack;
 unsigned index=0;
 for(const auto&fixture:cases){
  std::string dir="/tmp/case-"+std::to_string(index++);make(dir.c_str());FD directory(pathdir(dir)),archive(open(fixture.path,O_RDONLY|O_CLOEXEC)),log(open("/dev/null",O_WRONLY|O_CLOEXEC));
  check(archive.n>=0,"fixture-open");std::vector<Created> files;for(size_t i=0;i<fixture.count;i++)files.push_back({fixture.files+i,{},false});Decoder decoder;bool success=false;std::string failure;
  try{decoder.start(archive.n,log.n);extract(decoder,directory.n,files);success=true;}catch(const std::exception&e){failure=e.what();decoder.stop();}
  printf("UNPACK_CASE %s success=%d detail=%s\n",fixture.label,success,failure.c_str());check(success==fixture.success,fixture.label);
 }
}
static void full(const char*kind,const char*stage,const char*attempt){
 using namespace cn::unpack;int rc=run(kind,stage,attempt);check(rc==0,"full-unpack-success");FD parent(pathdir(stage));check(present(parent.n,"payload"),"payload-committed");check(present(parent.n,std::string("unpack.final.")+attempt),"final-written");
 bool refused=false;try{run(kind,stage,attempt);}catch(const cn::Error&e){refused=e.rc==3;}check(refused,"full-replay-refused");
}
static std::string fresh(unsigned id){char b[64];snprintf(b,sizeof b,"/data/local/tmp/x2d-un-%08x",id);make(b);cn::copy_new("/data/local/tmp/x2d-un-11223344/native-uninstall.gz",std::string(b)+"/native-uninstall.gz");return b;}
static void failures(){
 using namespace cn::unpack;
 auto stage=fresh(33);deny_commit=true;int rc=run("uninstall",stage,"3333333333333333");deny_commit=false;check(rc==1,"commit-failure-primary");FD dir(pathdir(stage));check(!present(dir.n,"payload")&&!present(dir.n,".payload-3333333333333333"),"failed-commit-owned-cleanup");check(cn::read(stage+"/unpack.final.3333333333333333").find("primary_errno=5\n")!=std::string::npos,"primary-errno-retained");
 stage=fresh(34);deny_commit=deny_cleanup=true;rc=run("uninstall",stage,"4444444444444444");deny_commit=deny_cleanup=false;check(rc==90,"cleanup-failure-90");auto receipt=cn::read(stage+"/unpack.final.4444444444444444");check(receipt.find("primary_rc=1\n")!=std::string::npos&&receipt.find("restore_rc=90\n")!=std::string::npos,"primary-and-restore-separated");
 stage=fresh(36);swap_directory=true;rc=run("uninstall",stage,"7777777777777777");check(rc==90,"renamed-directory-attack-rejected");receipt=cn::read(stage+"/unpack.final.7777777777777777");check(receipt.find("payload_ready=0\n")!=std::string::npos&&receipt.find("primary_rc=1\n")!=std::string::npos&&receipt.find("restore_rc=90\n")!=std::string::npos,"foreign-directory-never-ready");check(cn::read(stage+"/payload/foreign-marker")=="foreign","foreign-not-unlinked");check(cn::directory(stage+"/saved-verified"),"verified-original-retained");
 check(!rename("/system/xbin/busybox","/system/xbin/busybox.saved"),"decoder-move");int evil=open("/system/xbin/busybox",O_CREAT|O_EXCL|O_WRONLY,0700);check(evil>=0&&write(evil,"not-approved",12)==12,"decoder-replacement");close(evil);stage=fresh(35);rc=run("uninstall",stage,"5555555555555555");check(rc!=0,"decoder-hash-refused");check(cn::read(stage+"/unpack.final.5555555555555555").find("primary_step=unpack-decoder-digest\n")!=std::string::npos,"decoder-hash-receipt");check(!unlink("/system/xbin/busybox")&&!rename("/system/xbin/busybox.saved","/system/xbin/busybox"),"decoder-fixture-restore");
 for(unsigned mask=0;mask<8;mask++){
  stage=fresh(50+mask);pid_t pid=fork();check(pid>=0,"closedstdio-fork");if(!pid){for(unsigned fd=0;fd<3;fd++)if(mask&(1u<<fd))close(int(fd));int code=run("uninstall",stage,"6666666666666666");_exit(code);}int status;check(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"closedstdio-run");
 }
}
int main(){mount("devtmpfs","/dev","devtmpfs",0,nullptr);try{
 extract_cases();
 full("install-stock","/data/local/tmp/x2d-rs1","1111111111111111");
 full("uninstall","/data/local/tmp/x2d-un-11223344","2222222222222222");
 failures();
 using namespace cn::unpack;
 bool bad=false;try{run("uninstall","/data/local/tmp/x2d-un-11223344/../escape","3333333333333333");}catch(const cn::Error&e){bad=e.rc==2;}check(bad,"stage-traversal");
 const char*alias="/data/local/tmp/x2d-un-55667788";check(!symlink("x2d-un-11223344",alias),"stage-link-create");bad=false;try{run("uninstall",alias,"3333333333333333");}catch(const cn::Error&){bad=true;}check(bad,"stage-link-refused");
 stop_requested=1;bad=false;try{cn::cancelled();}catch(const cn::Error&e){bad=e.rc==143;}check(bad,"cancel-code");stop_requested=0;
 printf("NATIVE_UNPACK_LINUX_PASSED checks=%u\n",checks);fflush(nullptr);reboot(RB_POWER_OFF);return 0;
 }catch(const std::exception&e){printf("NATIVE_UNPACK_FAIL %s\n",e.what());fflush(nullptr);reboot(RB_POWER_OFF);return 1;}}
