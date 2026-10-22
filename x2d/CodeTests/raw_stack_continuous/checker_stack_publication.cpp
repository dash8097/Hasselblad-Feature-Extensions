/* 独立测试：冻结真实 stack runner 的释放/发布凭据 + 真实 common 文件实现。 */
#include "native_stack_runner.cpp"
#include <sys/reboot.h>
#include <sys/mman.h>
#include <cstdlib>
extern "C" int cn_cancel_requested(){return 0;}
extern "C" int cn_signal_pending(){return 0;}
extern "C" void cn_recovery_mode(int){}
static unsigned cases,checks;
static void ck(bool ok,const char*what){++checks;if(!ok){printf("STACK_PUBLICATION_FAIL case=%u %s\n",cases,what);fflush(stdout);reboot(RB_POWER_OFF);_exit(1);}}
static void dir(const std::string&p){ck(!mkdir(p.c_str(),0700)||errno==EEXIST,"mkdir fixture");}
static void file(const std::string&p,const std::string&v){int f=open(p.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);ck(f>=0,"open fixture");ck(write(f,v.data(),v.size())==(ssize_t)v.size(),"write fixture");ck(!fsync(f),"sync fixture");close(f);}
static std::string setup(const char*name){++cases;printf("STACK_PUBLICATION_CASE %u %s\n",cases,name);std::string s="/mnt/media_rw/ssd/job-"+std::to_string(cases);dir(s);file(s+"/output-plan","100HASBL/A0000001.JPG\n");file(s+"/raw.ready","RAW_STACK_R1\n");file(s+"/capture.log","RAW_STACK_FINAL_RAW_WRITTEN_ALL_SOURCE_REFERENCES_RELEASED\n");file(s+"/merged.3FR",std::string(4096,'R'));unlink("/mnt/media_rw/ssd/DCIM/100HASBL/A0000001.3FR");return s;}
template<class F>static void rejected(F fn,const char*name){bool caught=false;try{fn();}catch(const cn::Error&){caught=true;}ck(caught,name);}
static void publish(const std::string&s){cn::stack_runner::bind_raw_publication(s);ck(!rename((s+"/merged.3FR").c_str(),cn::stack_runner::raw_destination(s).c_str()),"actual rename");}
int main(){setvbuf(stdout,nullptr,_IONBF,0);ck(!mount("proc","/proc","proc",0,nullptr),"proc");ck(!mount("tmpfs","/tmp","tmpfs",0,nullptr),"tmp");for(auto p:{"/mnt/media_rw","/mnt/media_rw/ssd","/mnt/media_rw/ssd/DCIM","/mnt/media_rw/ssd/DCIM/100HASBL"})dir(p);using namespace cn::stack_runner;
 try{
 auto s=setup("staged complete");ck(natural_drained(s),"staged real parser accepts");
 s=setup("published same inode no album-location");publish(s);ck(natural_drained(s),"published proof survives index failure");ck(!cn::exists(s+"/album-location"),"no invented index receipt");
 s=setup("publisher rollback restores staged inode");publish(s);ck(!rename(raw_destination(s).c_str(),(s+"/merged.3FR").c_str()),"rollback rename");ck(natural_drained(s),"restored staged proof");
 s=setup("copied destination identical content");publish(s);auto target=raw_destination(s);ck(!rename(target.c_str(),(s+"/keep-old-inode").c_str()),"keep inode allocated");file(target,std::string(4096,'R'));ck(natural_drained(s),"copy with new inode accepted by content");file(target,std::string(4096,'X'));rejected([&]{natural_drained(s);},"same-sized different content rejected");
 s=setup("same inode truncated");publish(s);ck(!truncate(raw_destination(s).c_str(),8192),"resize");rejected([&]{natural_drained(s);},"size binding rejected");
 s=setup("changed canonical output plan");publish(s);file(s+"/output-plan","100HASBL/A0000002.JPG\n");rejected([&]{natural_drained(s);},"different plan rejected");
 s=setup("binding copied to another job");publish(s);auto second=s+"-other";dir(second);file(second+"/output-plan",cn::read(s+"/output-plan"));file(second+"/raw.ready",cn::read(s+"/raw.ready"));file(second+"/capture.log",cn::read(s+"/capture.log"));file(second+"/publish.raw.identity",cn::read(s+"/publish.raw.identity"));rejected([&]{natural_drained(second);},"job identity rejected");
 s=setup("missing release log after publication");publish(s);file(s+"/capture.log","");rejected([&]{natural_drained(s);},"missing release proof rejected");
 s=setup("duplicate release log");file(s+"/capture.log",cn::read(s+"/capture.log")+cn::read(s+"/capture.log"));rejected([&]{bind_raw_publication(s);},"ambiguous release rejected");
 s=setup("missing raw-ready after publication");publish(s);unlink((s+"/raw.ready").c_str());ck(!natural_drained(s),"missing raw-ready refuses");
 s=setup("empty RAW");file(s+"/merged.3FR","");rejected([&]{bind_raw_publication(s);},"empty raw rejected");
 s=setup("unaligned RAW");file(s+"/merged.3FR","X");rejected([&]{bind_raw_publication(s);},"unaligned raw rejected");
 s=setup("symlink RAW");ck(!rename((s+"/merged.3FR").c_str(),(s+"/actual").c_str()),"save source");ck(!symlink((s+"/actual").c_str(),(s+"/merged.3FR").c_str()),"symlink fixture");rejected([&]{bind_raw_publication(s);},"linked source rejected");
 s=setup("hardlinked RAW");ck(!link((s+"/merged.3FR").c_str(),(s+"/alias").c_str()),"hardlink fixture");rejected([&]{bind_raw_publication(s);},"multiply linked source rejected");
 s=setup("missing prepublication identity");ck(!rename((s+"/merged.3FR").c_str(),raw_destination(s).c_str()),"move without identity");rejected([&]{natural_drained(s);},"unbound destination rejected");
 s=setup("unsafe album plan");file(s+"/output-plan","../../outside\n");rejected([&]{bind_raw_publication(s);},"real common album parser rejects");
 s=setup("incomplete abort evidence");file(s+"/abort.drained","RAW_STACK_ABORT_DRAINED_V1\n");rejected([&]{aborted_drained(s);},"abort requires independent release log");
 s=setup("valid abort evidence");file(s+"/abort.drained","RAW_STACK_ABORT_DRAINED_V1\n");file(s+"/capture.log","RAW_STACK_ABORT_ALL_SOURCE_REFERENCES_AND_POOL_RELEASED\n");ck(aborted_drained(s),"valid abort pair");

 /* 真正的 /proc/self/maps：映射同前缀替身文件，不能靠字符串前缀通过。 */
 dir("/system");dir("/system/lib64");std::string q="/system/lib64/libx2d_quarter_native.so";file(q,std::string(4096,'Q'));file(q+".extra",std::string(4096,'Q'));struct stat qs;ck(!stat(q.c_str(),&qs),"quarter stat");
 ++cases;int fd=open((q+".extra").c_str(),O_RDONLY);ck(fd>=0,"extra open");void*map=mmap(nullptr,4096,PROT_READ,MAP_PRIVATE,fd,0);close(fd);ck(map!=MAP_FAILED,"extra mmap");ck(!mapped_module_identity(cn::read("/proc/self/maps",2097152),qs,q),"same prefix rejected");ck(!munmap(map,4096),"extra unmap");
 ++cases;fd=open(q.c_str(),O_RDONLY);ck(fd>=0,"quarter open");map=mmap(nullptr,4096,PROT_READ,MAP_PRIVATE,fd,0);close(fd);ck(map!=MAP_FAILED,"quarter mmap");auto maps=cn::read("/proc/self/maps",2097152);ck(mapped_module_identity(maps,qs,q),"actual exact mapping");
 ++cases;auto wrong=qs;wrong.st_ino++;rejected([&]{mapped_module_identity(maps,wrong,q);},"wrong inode rejected");
 ++cases;wrong=qs;wrong.st_dev=makedev(major(qs.st_dev)+1,minor(qs.st_dev));rejected([&]{mapped_module_identity(maps,wrong,q);},"wrong device rejected");
 ++cases;ck(!unlink(q.c_str()),"unlink mapped fixture");ck(!mapped_module_identity(cn::read("/proc/self/maps",2097152),qs,q),"deleted mapping rejected");ck(!munmap(map,4096),"quarter unmap");
 printf("STACK_PUBLICATION_ALL_PASSED cases=%u checks=%u\n",cases,checks);
 }catch(const std::exception&e){printf("STACK_PUBLICATION_UNEXPECTED %s\n",e.what());ck(false,"unexpected exception");}sync();reboot(RB_POWER_OFF);return 0;
}
