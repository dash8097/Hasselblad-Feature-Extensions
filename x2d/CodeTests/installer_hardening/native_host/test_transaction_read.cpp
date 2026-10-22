#include "native_transaction_read.cpp"
#include <sys/mount.h>
#include <sys/reboot.h>
static unsigned checks=0;
static void check(bool ok,const char*why){checks++;if(!ok)throw std::runtime_error(why);}
static void put(const std::string&p,const std::string&s){int f=open(p.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);check(f>=0,"fixture-create");check(write(f,s.data(),s.size())==ssize_t(s.size()),"fixture-write");close(f);}
int main(){mount("devtmpfs","/dev","devtmpfs",0,nullptr);try{using namespace native_host_read;
 for(const char*p:{"/data","/data/local","/data/local/tmp","/data/local/tmp/x2d-un-11223344"})mkdir(p,0700);
 const std::string stage="/data/local/tmp/x2d-un-11223344";FD fd(directory(stage));check(fd.n>=0,"stage-open");
 check(read(fd.n,"missing").state=="ABSENT","missing");put(stage+"/normal",std::string("abc\0def\n",8));auto result=read(fd.n,"normal");check(result.state=="PRESENT"&&result.data==std::string("abc\0def\n",8),"binary-preserved");check(b64(result.data)=="YWJjAGRlZgo=","base64");
 symlink("normal",(stage+"/symlink").c_str());check(read(fd.n,"symlink").state=="TYPE_REJECTED","symlink");link((stage+"/normal").c_str(),(stage+"/hardlink").c_str());check(read(fd.n,"hardlink").state=="TYPE_REJECTED","hardlink");unlink((stage+"/hardlink").c_str());
 mkfifo((stage+"/fifo").c_str(),0600);check(read(fd.n,"fifo").state=="TYPE_REJECTED","fifo-no-block");mkdir((stage+"/folder").c_str(),0700);check(read(fd.n,"folder").state=="TYPE_REJECTED","directory");put(stage+"/large",std::string(16385,'X'));check(read(fd.n,"large").state=="TOO_LARGE","oversize");check(read(-1,"file").state=="READ_ERROR","read-error");
 char*args[]={const_cast<char*>("checker"),const_cast<char*>("transaction-read"),const_cast<char*>("uninstall"),const_cast<char*>(stage.c_str()),const_cast<char*>("1234567890abcdef"),nullptr};
 auto empty=collect(5,args);check(empty.find("\"operation\":\"uninstall\"")!=std::string::npos&&empty.find("\"state\":\"ABSENT\"")!=std::string::npos,"fixed-envelope");
 put(stage+"/final","NATIVE_TRANSACTION_V1\noperation=uninstall\ntransaction=11223344\nattempt=1234567890abcdef\nprimary_rc=0\nrestore_rc=0\nfinal_rc=0\nprimary_step=none\nmanual_reboot_required=1\n");
 put(stage+"/dispatch.1234567890abcdef.claim","NATIVE_DISPATCH_V1\noperation=uninstall\nattempt=1234567890abcdef\n");put(stage+"/dispatch.1234567890abcdef.started","NATIVE_DISPATCH_STARTED_V1\noperation=uninstall\nattempt=1234567890abcdef\npid=123\nstart=456\n");put(stage+"/dispatch.1234567890abcdef.exit","0\n");
 printf("READER_SAMPLE=%s",collect(5,args).c_str());
 args[2]=const_cast<char*>("payload-unpack");put(stage+"/unpack.final.1234567890abcdef","unpack-only\n");check(collect(5,args).find(b64("unpack-only\n"))!=std::string::npos,"unpack-final-route");
 args[3]=const_cast<char*>("/data/local/tmp/x2d-rs1");check(collect(5,args).find("\"operation\":\"payload-unpack\"")!=std::string::npos,"unpack-install-stage");
 args[3]=const_cast<char*>("/data/local/tmp/x2d-tidy-11223344");bool unpack_bad=false;try{collect(5,args);}catch(const std::exception&){unpack_bad=true;}check(unpack_bad,"unpack-cleanup-stage-refused");args[2]=const_cast<char*>("uninstall");
 args[3]=const_cast<char*>("/data/local/tmp/x2d-un-11223344/../foreign");bool invalid=false;try{collect(5,args);}catch(const std::exception&){invalid=true;}check(invalid,"traversal-argument");
 rename(stage.c_str(),(stage+".saved").c_str());symlink((stage+".saved").c_str(),stage.c_str());args[3]=const_cast<char*>(stage.c_str());check(collect(5,args).find("TYPE_REJECTED")!=std::string::npos,"ancestor-link");
 puts("NATIVE_READER_ACTUAL_LINUX_PASSED");printf("checks=%u\n",checks);fflush(nullptr);reboot(RB_POWER_OFF);return 0;
 }catch(const std::exception&e){printf("NATIVE_READER_TEST_FAIL %s\n",e.what());fflush(nullptr);reboot(RB_POWER_OFF);return 1;}}
