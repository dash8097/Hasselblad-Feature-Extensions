#include <unistd.h>
#include <sys/syscall.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
static long intercepted_rename(long number,int a,const char*b,int c,const char*d,int flags){
 if(getenv("TEST_RETAIN_FAILURE")&&strstr(d,".previous.")){unsetenv("TEST_RETAIN_FAILURE");errno=EIO;return -1;}
 return syscall(number,a,b,c,d,flags);
}
static int intercepted_unlink(int dir,const char*name,int flags){
 if(!strcmp(name,"x2d-native-setup")){
  if(getenv("TEST_UNLINK_FAILURE")){errno=EIO;return -1;}
  if(getenv("TEST_TASK_REPLACE")){rename("/tmp/x2d-multishot-active","/tmp/saved-task");mkdir("/tmp/x2d-multishot-active",0700);}
  if(auto path=getenv("TEST_HANDOFF_BLOCK"))mkdir(path,0700);
  if(auto path=getenv("TEST_FINAL_BLOCK"))mkdir(path,0700);
  if(getenv("TEST_RO_BLOCK"))open("/system/bin/ro-blocker",O_RDWR);
 }
 return unlinkat(dir,name,flags);
}
#define syscall intercepted_rename
#define unlinkat intercepted_unlink
#include "native_bootstrap.cpp"
#undef syscall
#undef unlinkat
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "landing_command.hpp"
static unsigned checks=0;
extern "C" int test_mount(const char*a,const char*b,const char*c,unsigned long d,const void*e){
 if(getenv("TEST_MOUNT_ACCESS_DENIED")){unsetenv("TEST_MOUNT_ACCESS_DENIED");errno=EACCES;return -1;}
 return int(syscall(SYS_mount,a,b,c,d,e));
}
static int cancel_state=0,recovery_depth=0;
extern "C" int cn_cancel_requested(){return recovery_depth?0:cancel_state;}
extern "C" int cn_signal_pending(){return cancel_state;}
extern "C" void cn_recovery_mode(int on){recovery_depth+=on?1:-1;}
extern "C" int cn_job_valid(const char*){return 0;}
static void check(bool value,const char*name){checks++;if(!value)throw std::runtime_error(name);}
static void mkdirs(){for(const char*p:{"/tmp","/proc","/system","/bin","/data","/data/local","/data/local/tmp","/data/local/tmp/x2d-rs1"})check(!mkdir(p,0755)||errno==EEXIST,"fixture-dir");}
static void copy(const std::string&from,const std::string&to){cn::copy_new(from,to);check(!chmod(to.c_str(),0755),"fixture-exec-mode");}
static int execute(const std::string&seed,const std::string&attempt,const std::string&old,const std::string&digest,unsigned mask=0){pid_t pid=fork();check(pid>=0,"bootstrap-fork");if(!pid){for(unsigned i=0;i<3;i++)if(mask&(1u<<i))close(int(i));execl(seed.c_str(),seed.c_str(),"bootstrap-commit",attempt.c_str(),old.c_str(),digest.c_str(),nullptr);_exit(127);}int status;check(waitpid(pid,&status,0)==pid&&WIFEXITED(status),"bootstrap-child-wait");return WEXITSTATUS(status);}
static void cleanup_models(){
 const std::string target="/system/bin/x2d-native-setup",digest=cn::sha256("/init"),prior="abcdef0123456789",previous=target+".previous."+prior,seed="/system/bin/x2d-"+digest.substr(0,8);
 for(unsigned scenario=0;scenario<18;++scenario){
  for(const char*key:{"TEST_UNLINK_FAILURE","TEST_TASK_REPLACE","TEST_HANDOFF_BLOCK","TEST_FINAL_BLOCK","TEST_RO_BLOCK"})unsetenv(key);check(!mount("tmpfs","/system","tmpfs",0,"size=16m"),"cleanup-mount");check(!mkdir("/system/bin",0755),"cleanup-bin");check(!mkdir("/system/lib64",0755),"cleanup-lib");for(auto name:factory_files)copy(std::string("/factory")+name,std::string("/system")+name);copy("/init",target);std::string old="absent",previous_arg="none";
  if(scenario==8||scenario==11){cn::write_new(previous,"old-tool");check(!chmod(previous.c_str(),0755),"cleanup-old-mode");old=cn::sha256(previous);previous_arg=prior;copy("/init",seed);}
  if(scenario==9){cn::write_new("/system/bin/x2d-11111111","foreign");check(!chmod("/system/bin/x2d-11111111",0755),"cleanup-foreign-mode");}
  if(scenario==16)copy("/known-seed","/system/bin/x2d-a1815fca");
  if(scenario==17){cn::write_new("/system/bin/x2d-a1815fca","wrong-seed");check(!chmod("/system/bin/x2d-a1815fca",0755),"cleanup-known-seed-mode");}
  if(scenario==10){cn::write_new(previous,"unknown-retained");check(!chmod(previous.c_str(),0755),"cleanup-conflict-mode");old=std::string(64,'0');previous_arg=prior;}
  if(scenario==11)setenv("TEST_UNLINK_FAILURE","1",1);
  cn::write_new("/system/bin/ro-blocker","fixture");
  cn::bootstrap::remount(true);char attempt[32];snprintf(attempt,sizeof attempt,"%016x",100+scenario);
  if(scenario==12)setenv("TEST_TASK_REPLACE","1",1);
  if(scenario==13)setenv("TEST_HANDOFF_BLOCK",("/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.handoff."+std::string(attempt)+".next").c_str(),1);
  if(scenario==14)setenv("TEST_FINAL_BLOCK",("/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.final."+std::string(attempt)).c_str(),1);
  if(scenario==15)setenv("TEST_RO_BLOCK","1",1);
  pid_t pid=fork();check(pid>=0,"cleanup-fork");if(!pid){if(scenario<8)for(unsigned n=0;n<3;++n)if(scenario&(1u<<n))close(int(n));execl(target.c_str(),target.c_str(),"bootstrap-cleanup",attempt,digest.c_str(),old.c_str(),previous_arg.c_str(),nullptr);_exit(127);}int status;check(waitpid(pid,&status,0)==pid&&WIFEXITED(status),"cleanup-wait");int rc=WEXITSTATUS(status);printf("BOOTSTRAP_CLEANUP scenario=%u rc=%d\n",scenario,rc);if(scenario!=15)check(cn::bootstrap::mount_state().ro,"cleanup-readonly");else {check(rc==90&&!cn::bootstrap::mount_state().ro,"cleanup-ro-failure-reported");cn::bootstrap::remount(true);}
  if(scenario<=8||scenario==16){check(!cn::exists("/system/bin/x2d-a1815fca"),"cleanup-known-seed-removed");check(rc==0&&!cn::exists(target)&&!cn::exists(seed)&&!cn::exists(previous),"cleanup-removed-owned-tools");auto receipt=cn::read("/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.final."+std::string(attempt));check(receipt.find("tools_removed=1\n")!=std::string::npos&&receipt.find("final_rc=0\n")!=std::string::npos,"cleanup-receipt-readable-without-tool");check(cn::read("/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.final."+std::string(attempt)+".seal")==cn::sha256("/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.final."+std::string(attempt)),"cleanup-final-seal-matches");}
  else if(scenario<12||scenario==17){check(rc!=0&&cn::exists(target),"cleanup-failure-keeps-tool");if(scenario==11){auto receipt=cn::read("/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.final."+std::string(attempt));check(receipt.find("primary_errno=5\n")!=std::string::npos&&receipt.find("restore_rc=0\n")!=std::string::npos&&receipt.find("removed=2\n")!=std::string::npos,"cleanup-partial-primary-preserved");}}
  if(scenario>=12&&scenario<=15){check(rc==90&&!cn::exists(target),"cleanup-handoff-failure-reported");
   if(scenario!=14){check(cn::exists("/tmp/x2d-multishot-active"),"cleanup-uncertain-task-preserved");auto receipt=cn::read("/data/local/tmp/x2d-bootstrap/bootstrap.cleanup.final."+std::string(attempt));check(receipt.find("restore_rc=90\n")!=std::string::npos,"cleanup-final-not-success");check(!rmdir("/tmp/x2d-multishot-active"),"fixture-task-cleanup");}
   if(scenario==12)check(!rmdir("/tmp/saved-task"),"fixture-original-task-cleanup");
  }else check(!cn::exists("/tmp/x2d-multishot-active"),"cleanup-task-unlocked");check(!umount("/system"),"cleanup-unmount");
 }
 unsetenv("TEST_UNLINK_FAILURE");
}
static void landing(){
 for(const char*name:{"sh","mount","zcat","chmod","flock"}){std::string path="/bin/"+std::string(name);check(!symlink((std::string(name)=="sh"?"/system/bin/sh":"/system/bin/toybox"),path.c_str()),"applet-fixture");}
 setenv("PATH","/bin",1);const std::string target="/system/bin/x2d-12345678";
 // 使用实际量级的载荷，避免短字符串掩盖复制中途失败。
 std::string payload(1631680,'\0');for(size_t i=0;i<payload.size();++i)payload[i]=char(i%251);
 for(unsigned mode=0;mode<9;mode++){
  unsetenv("TEST_MOUNT_FAIL");unsetenv("TEST_CAT_DELAY");unsetenv("TEST_CHMOD_FAIL");
  if(mode==3){check(!unlink("/bin/zcat"),"replace-cat-link");copy("/init","/bin/zcat");setenv("TEST_CAT_DELAY","1",1);}
  if(mode==4){check(!unlink("/bin/mount"),"replace-mount-link");copy("/init","/bin/mount");setenv("TEST_MOUNT_FAIL","ro",1);}
  if(mode==5)setenv("TEST_MOUNT_FAIL","rw",1);
  if(mode==6){check(!unlink("/bin/chmod"),"replace-chmod-link");copy("/init","/bin/chmod");setenv("TEST_CHMOD_FAIL","1",1);}
  check(!mount("tmpfs","/system","tmpfs",0,mode==8?"size=8m":"size=16m"),"landing-mount");check(!mkdir("/system/bin",0755),"landing-bin");check(!mkdir("/system/lib64",0755),"landing-lib");for(auto name:factory_files)copy(std::string("/factory")+name,std::string("/system")+name);
  if(mode==1)cn::write_new(target,"foreign-existing");if(mode!=2)cn::write_new("/data/local/tmp/x2d-rs1/z12345678.gz",cn::read("/landing-gzip"));cn::bootstrap::remount(true);
  int held=-1;if(mode==7){held=open("/tmp/x2dt",O_RDONLY|O_CLOEXEC);check(held>=0&&!flock(held,LOCK_EX|LOCK_NB),"landing-busy-lock");}
  pid_t child=fork();check(child>=0,"landing-fork");if(!child){execl("/bin/sh","sh","-c",landing_command,nullptr);_exit(127);}if(mode==3){for(unsigned n=0;n<1000&&!cn::exists("/tmp/cat-ready");++n)usleep(1000);check(cn::exists("/tmp/cat-ready"),"landing-cat-ready");check(!kill(child,SIGTERM),"landing-cancel");}int status;check(waitpid(child,&status,0)==child&&WIFEXITED(status),"landing-wait");int rc=WEXITSTATUS(status);printf("BOOTSTRAP_LANDING mode=%u rc=%d bytes=%zu\n",mode,rc,strlen(landing_command));if(held>=0)close(held);if(mode!=4)check(cn::bootstrap::mount_state().ro,"landing-always-readonly");
  if(mode==0){check(rc==0&&cn::read(target,payload.size())==payload,"landing-success");struct stat st;check(!stat(target.c_str(),&st)&&(st.st_mode&0777)==0755,"landing-executable-mode");}
  else if(mode==1)check(rc!=0&&cn::read(target)=="foreign-existing","landing-noclobber");else if(mode==2)check(rc!=0,"landing-missing-source-fails");else if(mode==3)check(rc==143,"landing-signal-reported");else if(mode==4){check(rc==90&&!cn::bootstrap::mount_state().ro,"landing-readonly-failure-90");cn::bootstrap::remount(true);}else if(mode==5)check(rc==9&&!cn::exists(target),"landing-rw-failure");else if(mode==6)check(rc==8,"landing-chmod-failure");else if(mode==7)check(rc!=0&&!cn::exists(target),"landing-lock-refused-before-write");
  if(mode==8){check(rc!=0,"landing-full-device-rejected");check(cn::exists(target)&&cn::read(target,payload.size()).size()<payload.size(),"landing-partial-preserved");}
  if(mode!=2)check(!unlink("/data/local/tmp/x2d-rs1/z12345678.gz"),"landing-fixture-input-cleanup");check(!umount("/system"),"landing-unmount");
 }
}
static void network_model(){
 check(!mount("tmpfs","/system","tmpfs",0,"size=16m"),"network-mount");
 check(!mkdir("/system/bin",0755)&&!mkdir("/system/lib64",0755),"network-dirs");
 for(auto name:factory_files)copy(std::string("/factory")+name,std::string("/system")+name);
 check(!symlink("/system/bin/toybox","/bin/toybox"),"network-toybox");
 check(!symlink("/system/bin/toybox","/bin/nohup"),"network-nohup");
 check(!symlink("/system/bin/toybox","/bin/cat"),"network-cat");
 check(system("/busybox ifconfig lo up")==0,"network-loopback");
 check(system(network_command)==0,"network-dispatch");
 int fd=-1;bool connected=false;sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(30306);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 for(unsigned n=0;n<100&&!connected;n++){if(fd>=0)close(fd);fd=socket(AF_INET,SOCK_STREAM,0);check(fd>=0,"network-socket");connected=connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof addr)==0;if(!connected)usleep(10000);}
 check(connected,"network-connected");timeval timeout{5,0};check(!setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout),"network-timeout");
 std::string data(773779,'x');size_t offset=0;while(offset<data.size()){auto sent=send(fd,data.data()+offset,data.size()-offset,MSG_NOSIGNAL);check(sent>0,"network-send");offset+=size_t(sent);}
 check(!shutdown(fd,SHUT_WR),"network-shutdown");char byte;check(recv(fd,&byte,1,0)==0,"network-eof");close(fd);
 for(unsigned n=0;n<100&&!cn::exists("/data/local/tmp/x2d-rs1/z12345678.rc");n++)usleep(10000);
 check(cn::read("/data/local/tmp/x2d-rs1/z12345678.rc")=="0\n","network-receiver-success");
 auto received=cn::read("/data/local/tmp/x2d-rs1/z12345678.gz",data.size());printf("NETWORK bytes=%zu expected=%zu\n",received.size(),data.size());check(received==data,"network-exact-content");
 setenv("TEST_MOUNT_ACCESS_DENIED","1",1);check(!cn::system_remount(MS_REMOUNT|MS_RDONLY),"factory-remount-ro-after-denied-syscall");check(cn::bootstrap::mount_state().ro,"factory-remount-ro-confirmed");
 setenv("TEST_MOUNT_ACCESS_DENIED","1",1);check(!cn::system_remount(MS_REMOUNT),"factory-remount-rw-after-denied-syscall");check(!cn::bootstrap::mount_state().ro,"factory-remount-rw-confirmed");
 check(!cn::system_remount(MS_REMOUNT|MS_RDONLY),"factory-final-ro");
 check(!umount("/system"),"network-unmount");
}
int main(int argc,char**argv){
 if(strstr(argv[0],"/bin/zcat")||!strcmp(argv[0],"zcat")){if(getenv("TEST_CAT_DELAY")){cn::write_new("/tmp/cat-ready","ready");usleep(200000);}execv("/busybox",argv);return 127;}
 if(strstr(argv[0],"/bin/mount")||!strcmp(argv[0],"mount")){if(argc>1&&!strcmp(argv[1],"-oremount,ro")){int verify=open("/tmp/x2dt",O_RDONLY|O_CLOEXEC);if(verify<0)return 18;int rc=flock(verify,LOCK_EX|LOCK_NB);close(verify);if(!rc){fprintf(stderr,"LOCK_RELEASED_BEFORE_RO\n");return 18;}}const char*fail=getenv("TEST_MOUNT_FAIL");if(fail&&argc>1&&std::string(argv[1])=="-oremount,"+std::string(fail))return 9;execv("/busybox",argv);return 127;}
 if(strstr(argv[0],"/bin/chmod")||!strcmp(argv[0],"chmod")){if(getenv("TEST_CHMOD_FAIL"))return 8;execv("/busybox",argv);return 127;}
 if(strstr(argv[0],"dbus-send")){const std::string property=argc>1?argv[argc-1]:"";if(property=="string:active_error")puts("variant boolean false");else printf("variant int32 %s\n",property=="string:system_state"?"1":"0");return 0;}
 if(argc>1)return cn_bootstrap_main(argc,argv);
 mount("devtmpfs","/dev","devtmpfs",0,nullptr);try{mkdirs();check(!mount("proc","/proc","proc",0,nullptr),"proc-mount");copy("/init","/bin/dbus-send");
 const auto digest=cn::sha256("/init"),seed="/system/bin/x2d-"+digest.substr(0,8);
 for(unsigned scenario=0;scenario<11;scenario++){
  check(!mount("tmpfs","/system","tmpfs",0,"size=16m"),"system-mount");check(!mkdir("/system/bin",0755),"bin-mkdir");copy("/init",seed);std::string old="absent";bool update=scenario>=8;
  if(update){cn::write_new("/system/bin/x2d-native-setup","old-system-program\n");check(!chmod("/system/bin/x2d-native-setup",0755),"old-mode");old=cn::sha256("/system/bin/x2d-native-setup");}
  if(scenario==9)setenv("TEST_RETAIN_FAILURE","1",1);else unsetenv("TEST_RETAIN_FAILURE");
  if(scenario==10)old=std::string(64,'0');
  cn::bootstrap::remount(true);char attempt_buffer[32];snprintf(attempt_buffer,sizeof attempt_buffer,"%016x",scenario+1);std::string attempt=attempt_buffer;
  int rc=execute(seed,attempt,old,digest,scenario<8?scenario:0);printf("BOOTSTRAP_MODEL scenario=%u rc=%d\n",scenario,rc);check(cn::bootstrap::mount_state().ro,"system-restored-readonly");
  if(scenario<9){check(rc==0,"bootstrap-success");check(cn::sha256("/system/bin/x2d-native-setup")==digest,"target-exact-digest");check(!cn::exists(seed),"seed-consumed");check(cn::bootstrap::collect(attempt).find("PRESENT")!=std::string::npos,"read-final");}
  else if(scenario==9){check(rc==1,"retention-failure-primary");check(cn::sha256("/system/bin/x2d-native-setup")==old,"rollback-old-inode-content");check(cn::sha256(seed)==digest,"rollback-seed");auto final=cn::read("/data/local/tmp/x2d-bootstrap/bootstrap.final."+attempt);check(final.find("primary_errno=5\n")!=std::string::npos&&final.find("restore_rc=0\n")!=std::string::npos,"rollback-receipt");}
  else check(rc!=0&&cn::sha256(seed)==digest,"unknown-old-preserved");
  check(!cn::exists("/tmp/x2d-multishot-active"),"task-lock-released");check(!umount("/system"),"system-unmount");
 }
 landing();cleanup_models();network_model();
 printf("NATIVE_BOOTSTRAP_LINUX_PASSED checks=%u\n",checks);fflush(nullptr);reboot(RB_POWER_OFF);return 0;
 }catch(const std::exception&e){printf("NATIVE_BOOTSTRAP_LINUX_FAIL %s\n",e.what());fflush(nullptr);reboot(RB_POWER_OFF);return 1;}}
