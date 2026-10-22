/* 实现者离线合同测试；用实际 Linux 子进程，不执行相机/Qt 调用。 */
#include "native_stack_runner.cpp"
#include <sys/reboot.h>
#include <ctime>
extern "C" int checker_stack_abort(void);
extern "C" int cn_signal_pending(void){return 0;}
namespace cn {
static unsigned stop_calls,pump_calls;static bool simulated_exposure,stop_rejected;
uint64_t monotonic_ms(){timespec t{};require(!clock_gettime(CLOCK_MONOTONIC,&t),"test-clock");return uint64_t(t.tv_sec)*1000+t.tv_nsec/1000000;}
std::string get(const std::string&object,const std::string&property,const std::string&,unsigned timeout){
 require(timeout>0&&timeout<=179000,"command-bounds",2);
 if(property=="capture_storage_mode")++pump_calls;
 require((object=="camera"&&property=="exposure_status")||(object=="storage"&&(property=="storage_status"||property=="capture_storage_mode")),"unexpected-test-property");
 return property=="exposure_status"&&simulated_exposure?"1":"0";
}
Process command(const std::vector<std::string>&args,unsigned,bool){
 require(args.size()==7&&args.back()=="com.hasselblad.camera.stop_exposing","unexpected-test-command");
 stop_calls++;if(!stop_rejected)simulated_exposure=false;return {stop_rejected?1:0,{}};
}
void write_replace(const std::string&p,const std::string&s){int f=open(p.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0600);require(f>=0&&write(f,s.data(),s.size())==ssize_t(s.size()),"test-replace");close(f);}
void require(bool ok,const std::string&step,int rc){if(!ok)throw Error(step,rc);}void require(bool ok,const char*step,int rc){if(!ok)throw Error(step,rc);}
bool digits(const std::string&s){if(s.empty())return false;for(char c:s)if(c<'0'||c>'9')return false;return true;}
bool stopping(){return false;}void sleep_ms(unsigned ms){timespec ts{time_t(ms/1000),long(ms%1000)*1000000};while(nanosleep(&ts,&ts))require(errno==EINTR,"test-sleep");}
std::string read(const std::string&p,size_t limit){int fd=open(p.c_str(),O_RDONLY|O_CLOEXEC);require(fd>=0,"test-read");std::string out;char b[4096];for(;;){auto n=::read(fd,b,sizeof b);if(n<0&&errno==EINTR)continue;require(n>=0,"test-read");if(!n)break;out.append(b,size_t(n));require(out.size()<=limit,"test-read-limit");}close(fd);return out;}
// 内容摘要桩只验证调用契约；真实 SHA-256 使用生产 common 实现。
std::string sha256(const std::string&p){auto bytes=read(p,16384);unsigned long long h=1469598103934665603ULL;for(unsigned char c:bytes)h=(h^c)*1099511628211ULL;return std::to_string(h);}
bool exists(const std::string&p){struct stat st;return !lstat(p.c_str(),&st);}
void mkdir_new(const std::string&p){require(!mkdir(p.c_str(),0700),"test-exclusive-directory");}
bool regular(const std::string&p,bool nonempty){struct stat st;return !lstat(p.c_str(),&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1&&(!nonempty||st.st_size>0);}
std::string value(const std::string&p,size_t limit){auto s=read(p,limit);while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.pop_back();return s;}
void write_new(const std::string&p,const std::string&s){int f=open(p.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);require(f>=0,"test-create");require(write(f,s.data(),s.size())==ssize_t(s.size())&&!fsync(f),"test-write");close(f);}
Album album_plan(const std::string&s){require(s=="TEST_PLAN_A"||s=="TEST_PLAN_B","test-plan-parser");return {"100HASBL",s=="TEST_PLAN_A"?"B0000001":"B0000002",100,1};}
unsigned log_count(const std::string&p,const std::string&needle){std::istringstream lines(read(p,16*1024*1024));std::string line;unsigned count=0;while(std::getline(lines,line))if(line==needle)count++;return count;}
}
static unsigned checks=0;
static void check(bool value,const char*why){checks++;if(!value)throw std::runtime_error(why);}
int main(){using namespace cn;using namespace cn::stack_runner;mkdir("/proc",0755);mount("proc","/proc","proc",0,nullptr);mount("devtmpfs","/dev","devtmpfs",0,nullptr);
 try{
  mkdir("/tmp/lock-test",0700);
  {DirOwner owned("/tmp/lock-test/owned");owned.acquire();check(owned.fd>=0&&fcntl(owned.fd,F_GETFD)&FD_CLOEXEC,"directory-identity-pinned-close-on-exec");struct stat pinned{};check(!fstat(owned.fd,&pinned)&&pinned.st_ino==owned.id.st_ino,"pinned-directory-identity");owned.release();check(!exists(owned.path)&&owned.fd<0,"owned-empty-directory-released");}
  {DirOwner owned("/tmp/lock-test/replaced");owned.acquire();require(!rename(owned.path.c_str(),"/tmp/lock-test/original"),"test-rename-owned-lock");mkdir(owned.path.c_str(),0700);bool refused=false;try{owned.release();}catch(const Error&e){refused=e.rc==90;}check(refused&&exists(owned.path)&&exists("/tmp/lock-test/original"),"replacement-keeps-both-directories");}
  {symlink("/tmp/lock-test/original","/tmp/lock-test/symlink");DirOwner owned("/tmp/lock-test/symlink");owned.held=true;bool refused=false;try{owned.pin();}catch(const Error&e){refused=e.rc==90;}check(refused&&exists(owned.path),"directory-symlink-never-acquired");}
  poll_hook();check(pump_calls==1,"poll-hook-reaches-dbus-with-cleanup-time-budget");
  pump();check(pump_calls==2,"recovery-pump-must-not-silently-skip-dispatch");
  {
   struct Probe {
    uint64_t clock=0;unsigned reads=0,guards=0,sleeps=0;std::vector<AdmissionState> states;
    unsigned fail_guard=UINT_MAX,fail_read=UINT_MAX;uint64_t read_ms=0;
    AdmissionState run(){return await_owned_idle([&](){if(reads==fail_read)throw Error("injected-access-denied",7,false,EACCES);auto i=reads++;clock+=read_ms;return states[std::min<size_t>(i,states.size()-1)];},[&](){return clock;},[&](unsigned ms){check(ms==100,"admission-bounded-pause");sleeps++;clock+=ms;},[&](){if(guards++==fail_guard)throw Error("injected-cancel-or-owner-loss",71);});}
   };
   const AdmissionState zero{"0","false","0"},startup{"0","false","128"},busy{"1","false","0"};
   check(!(startup.exposure=="0"&&startup.error=="false"&&startup.storage=="0"),"old-instant-admission-reproduces-observed-128-rejection");
   for(unsigned method=0;method<3;method++){
    Probe p;p.states={startup,startup,zero};auto result=p.run();
    check(result.storage=="0"&&p.reads==5&&p.clock==400&&p.guards==5,"all-methods-startup-must-reach-three-idle-samples");
   }
   {Probe p;p.states={zero};p.run();check(p.reads==3&&p.clock==200,"already-idle-still-stable-before-ready");}
   {Probe p;p.states={zero,startup,zero,busy,zero};p.run();check(p.reads==7,"busy-or-startup-resets-stability");}
   {Probe p;p.states={startup};bool refused=false;try{p.run();}catch(const Error&e){refused=e.rc==19&&std::string(e.what())=="stack-storage-ready-timeout";}check(refused&&p.clock==5000&&p.reads==50,"permanent-storage-not-ready-times-out-without-admission");}
   {Probe p;p.states={busy};bool refused=false;try{p.run();}catch(const Error&e){refused=e.rc==19;}check(refused&&p.reads==50,"permanent-exposure-busy-never-admitted");}
   for(const auto&bad:std::vector<AdmissionState>{{"0","true","0"},{"0","true","128"},{"0","unknown","0"},{"0","false","invalid"},{"invalid","false","0"}}){Probe p;p.states={bad};bool refused=false;try{p.run();}catch(const Error&e){refused=e.rc==19;}check(refused&&p.reads==1&&!p.sleeps,"native-error-or-malformed-state-fails-immediately");}
   {Probe p;p.states={startup,zero};p.fail_guard=1;bool refused=false;try{p.run();}catch(const Error&e){refused=e.rc==71;}check(refused&&p.reads==1,"cancel-or-owner-loss-stops-before-another-read");}
   {Probe p;p.states={startup,zero};p.fail_read=1;bool refused=false;try{p.run();}catch(const Error&e){refused=e.rc==7&&e.system_errno==EACCES;}check(refused&&p.reads==1,"dbus-access-or-read-failure-propagates-without-retry");}
   {Probe p;p.states={zero};p.read_ms=5000;bool refused=false;try{p.run();}catch(const Error&e){refused=e.rc==19;}check(refused&&p.reads==1&&!p.sleeps,"late-idle-read-cannot-admit-after-deadline");}
   puts("STACK_ADMISSION_REGRESSION_PASSED cases=16");
  }
  for(unsigned method=0;method<3;method++)for(unsigned noise=0;noise<=0;noise++){auto c=config(std::to_string(method),"10",std::to_string(noise),"0");check(c.method==method&&c.seconds==10&&!c.target_iso&&c.noise==noise,"method/noise");}
  auto iso_cfg=config("3","100","0","60");check(iso_cfg.method==2&&iso_cfg.target_iso==100&&iso_cfg.seconds==86399&&iso_cfg.delay==60,"iso-mode");
  for(auto args:std::vector<std::array<std::string,4>>{{"0","10","1","0"},{"1","10","1","0"},{"2","10","1","0"},{"3","100","1","0"},{"4","1","0","0"},{"0","0","0","0"},{"0","86400","0","0"},{"3","65536","0","0"},{"3","0100","0","0"},{"0","10","0","61"}}){bool failed=false;try{config(args[0],args[1],args[2],args[3]);}catch(const Error&){failed=true;}check(failed,"invalid-input-rejected");}
  auto plan=parse_iso("RSIP1 800 100 8 1 125\n",100);check(plan.frames==8&&plan.source==800&&plan.denominator==125,"iso-plan");
  for(const auto*s:{"RSIP1 800 100 7 1 125\n","RSIP1 800 200 4 1 125\n","RSIP1 100 800 1 1 125\n","RSIP1 800 100 8 0 125\n","RSIP1 800 100 8 1 4294967296\n","RSIP1 0800 100 8 1 125\n","RSIP1 800 100 8 1 125","RSIP1 800 100 8 1 125\n\n"}){bool failed=false;try{parse_iso(s,100);}catch(const Error&){failed=true;}check(failed,"bad-iso-plan-rejected");}
  Config time_cfg=config("0","10","0","2");IsoPlan none;
  check(continue_frames(0,999,1000,false,time_cfg,none),"time-first-frame");check(!continue_frames(0,1000,1000,false,time_cfg,none),"time-deadline");check(!continue_frames(2,1,1000,true,time_cfg,none),"manual-finish");check(!continue_frames(65535,1,1000,false,time_cfg,none),"frame-limit");check(continue_frames(0,999999,1,false,iso_cfg,none),"iso-first-no-plan");check(continue_frames(7,999999,1,false,iso_cfg,plan)&&!continue_frames(8,0,1,false,iso_cfg,plan),"iso-planned-bound");check(!continue_frames(1,0,1,true,iso_cfg,plan),"iso-early-finish");
  validate({"6","3","16","4"});
  mkdir("/tmp/stack-test",0700);mkdir("/mnt",0700);mkdir("/mnt/media_rw",0700);mkdir("/mnt/media_rw/ssd",0700);mkdir("/mnt/media_rw/ssd/DCIM",0700);mkdir("/mnt/media_rw/ssd/DCIM/100HASBL",0700);
  const std::string s="/tmp/stack-test",raw=s+"/merged.3FR",dst="/mnt/media_rw/ssd/DCIM/100HASBL/B0000001.3FR";
  write_new(s+"/raw.ready","RAW_STACK_R1\n");write_new(s+"/capture.log","RAW_STACK_FINAL_RAW_WRITTEN_ALL_SOURCE_REFERENCES_RELEASED\n");write_new(s+"/output-plan","TEST_PLAN_A\n");write_new(raw,std::string(4096,'R'));
  check(natural_drained(s),"raw-staged-release");bind_raw_publication(s);check(natural_drained(s),"raw-bound-staged");require(!rename(raw.c_str(),dst.c_str()),"test-publish-rename");check(natural_drained(s)&&!exists(s+"/album-location"),"raw-published-before-index-receipt");
  require(!rename(dst.c_str(),(dst+".copy-source").c_str()),"copy-source-retain");write_new(dst,read(dst+".copy-source",8192));unlink((dst+".copy-source").c_str());check(natural_drained(s),"copy-publish-new-inode-same-content");
  auto rejected_raw=[&](){try{(void)natural_drained(s);return false;}catch(const Error&){return true;}};
  require(!rename(dst.c_str(),(dst+".saved").c_str()),"test-retain-original");write_new(dst,std::string(4096,'X'));check(rejected_raw(),"raw-replaced-inode-rejected");unlink(dst.c_str());rename((dst+".saved").c_str(),dst.c_str());
  require(!truncate(dst.c_str(),8192),"test-truncate");check(rejected_raw(),"raw-size-change-rejected");truncate(dst.c_str(),4096);
  rename(dst.c_str(),(dst+".saved").c_str());symlink((dst+".saved").c_str(),dst.c_str());check(rejected_raw(),"raw-symlink-rejected");unlink(dst.c_str());rename((dst+".saved").c_str(),dst.c_str());
  rename((s+"/publish.raw.identity").c_str(),(s+"/binding.saved").c_str());check(rejected_raw(),"raw-missing-binding-rejected");rename((s+"/binding.saved").c_str(),(s+"/publish.raw.identity").c_str());
  rename((s+"/capture.log").c_str(),(s+"/capture.saved").c_str());write_new(s+"/capture.log","\n");check(rejected_raw(),"raw-release-evidence-still-required");unlink((s+"/capture.log").c_str());rename((s+"/capture.saved").c_str(),(s+"/capture.log").c_str());check(natural_drained(s),"raw-committed-error-cleanup-drain");
  struct stat fake{};fake.st_dev=makedev(8,2);fake.st_ino=321;auto maps=std::string("1000-2000 r-xp 00000000 08:02 321 ")+module+"\n";check(mapped_module_identity(maps,fake),"module-exact-map");fake.st_ino=322;bool map_bad=false;try{mapped_module_identity(maps,fake);}catch(const Error&){map_bad=true;}check(map_bad,"module-old-inode-rejected");check(!mapped_module_identity(maps.substr(0,maps.size()-1)+" (deleted)\n",fake),"module-deleted-map-rejected");
  mkdir("/tmp/burst-test",0700);const std::string burst="/tmp/burst-test";
  stop_continuous(burst);check(stop_calls==0,"no-active-no-stop");
  write_new(burst+"/burst.active","1\n");simulated_exposure=true;stop_continuous(burst);
  check(stop_calls==1&&exact(burst+"/burst.closed","RSB1 1")&&exact(burst+"/burst.stopped","RSB1 1"),"stop-confirmed-then-close");
  stop_continuous(burst);check(stop_calls==1,"stop-idempotent");
  write_replace(burst+"/burst.active","2\n");simulated_exposure=stop_rejected=true;bool stop_bad=false;
  try{stop_continuous(burst);}catch(const Error&){stop_bad=true;}
  check(stop_bad&&stop_calls==2&&exact(burst+"/burst.closed","RSB1 1"),"failed-stop-never-close-new-epoch");stop_rejected=false;
  ProcessChild in_flight;in_flight.launch("/tmp/in-flight.log",[](){sleep_ms(100);return 0;});
  write_new(burst+"/capture.rpc.pid",std::to_string(in_flight.pid)+"\n");write_new(burst+"/capture.rpc.start",in_flight.start+"\n");
  stop_continuous(burst);check(in_flight.poll()&&in_flight.rc==0&&exact(burst+"/burst.closed","RSB1 2"),"close-waits-for-rpc-incarnation");
  ProcessChild child;child.launch("/tmp/child.log",[](){return 19;});check(child.wait()==19,"actual-child-result");
  ProcessChild gated;bool rejected=false;try{gated.launch("/tmp/gate.log",[](){int fd=open("/tmp/forbidden",O_CREAT|O_WRONLY,0600);if(fd>=0)close(fd);return 0;},[](){throw Error("injected-receipt-failure",44);});}catch(const Error&e){rejected=e.rc==44;}check(rejected&&gated.wait()==126&&access("/tmp/forbidden",F_OK)!=0,"gate-refuses-exec-after-registration-failure");
  ProcessChild owner;owner.launch("/tmp/owner.log",[](){return 0;});wait_storage_exit(owner.pid,owner.start);check(owner.poll()&&owner.rc==0,"restore-leaves-reap-to-owner");
  check(checker_stack_abort()==0,"abort-model");printf("STACK_RUNNER_CONTRACT_PASSED checks=%u\n",checks);fflush(nullptr);reboot(RB_POWER_OFF);return 0;
 }catch(const std::exception&e){fprintf(stderr,"STACK_CONTRACT_FAIL %s\n",e.what());fflush(nullptr);reboot(RB_POWER_OFF);return 1;}
}
