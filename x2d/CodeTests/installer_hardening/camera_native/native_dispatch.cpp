/* 固定事务异步派发。USB 回包丢失不影响已经派发的唯一事务，也不重发。 */
#include "native_common.hpp"
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <sstream>
extern "C" int cn_install_main(int,char**);
extern "C" int cn_uninstall_main(int,char**);
extern "C" int cn_setup_main(int,char**);
extern "C" int cn_data_cleanup_main(int,char**);
#ifdef CN_PACKAGED_PAYLOAD
extern "C" int cn_unpack_main(int,char**);
#endif
namespace cn { namespace dispatch {
struct FD {int n=-1;explicit FD(int fd=-1):n(fd){}~FD(){if(n>=0)close(n);}FD(const FD&)=delete;void reset(){if(n>=0)close(n);n=-1;}};
static void ok(bool yes,const char*name,int rc=1){if(!yes)throw Error(name,rc,false,errno);}
static bool hex(const std::string&s,size_t n){return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static int high(int fd){if(fd<0||fd>=3)return fd;int copy=fcntl(fd,F_DUPFD_CLOEXEC,3);int cause=errno;close(fd);if(copy<0)throw Error("dispatch-descriptor-duplicate",1,false,cause);return copy;}
static int pathdir(const std::string&path){int dir=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);ok(dir>=0,"dispatch-root");try{std::istringstream parts(path.substr(1));std::string name;while(std::getline(parts,name,'/')){require(!name.empty()&&name!="."&&name!="..","dispatch-path-part",2);int next=openat(dir,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);ok(next>=0,"dispatch-stage");close(dir);dir=next;}return dir;}catch(...){close(dir);throw;}}
static void create(int stage,const std::string&name,const std::string&text){FD file(openat(stage,name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));ok(file.n>=0,"dispatch-exclusive-record");size_t at=0;while(at<text.size()){ssize_t n=write(file.n,text.data()+at,text.size()-at);if(n<0&&errno==EINTR)continue;ok(n>0,"dispatch-record-write");at+=size_t(n);}ok(!fsync(file.n)&&!fsync(stage),"dispatch-record-sync");}
static std::string identity(pid_t pid){std::string stat=read("/proc/"+std::to_string(pid)+"/stat",16384);auto end=stat.rfind(')');require(end!=std::string::npos,"dispatch-process-stat");std::istringstream fields(stat.substr(end+1));std::string value;for(unsigned f=3;f<=22;++f)require(bool(fields>>value),"dispatch-process-fields");require(digits(value),"dispatch-starttime");return value;}
static int call(int(*entry)(int,char**),std::vector<std::string> args){std::vector<char*>argv;for(auto&s:args)argv.push_back(&s[0]);argv.push_back(nullptr);return entry(int(args.size()),argv.data());}
static int run(int argc,char**argv){
    require(argc>=5&&std::string(argv[1])=="dispatch","dispatch-arguments",2);std::string action=argv[2],stage,attempt;int(*entry)(int,char**)=nullptr;
    if(action=="install-stock"){require(argc==5&&(std::string(argv[3])=="255"||std::string(argv[3])=="248"),"dispatch-install-arguments",2);entry=cn_install_main;stage="/data/local/tmp/x2d-rs1";attempt=argv[4];}
    else if(action=="uninstall"||action=="retained-cleanup"){require(argc==5,"dispatch-uninstall-arguments",2);stage=argv[3];attempt=argv[4];const std::string prefix="/data/local/tmp/x2d-un-";require(stage.rfind(prefix,0)==0&&hex(stage.substr(prefix.size()),8),"dispatch-uninstall-stage",2);entry=action=="uninstall"?cn_uninstall_main:cn_setup_main;}
    else if(action=="data-cleanup"){require(argc==6,"dispatch-cleanup-arguments",2);std::string mode=argv[3];require(mode=="before"||mode=="installed"||mode=="uninstalled","dispatch-cleanup-mode",2);stage=argv[4];attempt=argv[5];const std::string prefix="/data/local/tmp/x2d-tidy-";require(stage.rfind(prefix,0)==0&&hex(stage.substr(prefix.size()),8),"dispatch-cleanup-stage",2);entry=cn_data_cleanup_main;}
#ifdef CN_PACKAGED_PAYLOAD
    else if(action=="payload-unpack"){require(argc==6,"dispatch-unpack-arguments",2);std::string kind=argv[3];stage=argv[4];attempt=argv[5];const std::string prefix="/data/local/tmp/x2d-un-";require((kind=="install-stock"&&stage=="/data/local/tmp/x2d-rs1")||(kind=="uninstall"&&stage.rfind(prefix,0)==0&&hex(stage.substr(prefix.size()),8)),"dispatch-unpack-stage",2);entry=cn_unpack_main;}
#endif
    else throw Error("dispatch-operation",2);
    require(hex(attempt,16),"dispatch-attempt",2);cancelled();FD dir(high(pathdir(stage)));const std::string prefix="dispatch."+attempt;
    // claim 在 fork 前持久化；发送丢失或父进程退出后，同 attempt 仍不能重复派发。
    create(dir.n,prefix+".claim","NATIVE_DISPATCH_V1\noperation="+action+"\nattempt="+attempt+"\n");
    FD log(high(openat(dir.n,(prefix+".log").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600)));ok(log.n>=0,"dispatch-log");
    int sockets[2];ok(!socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,sockets),"dispatch-gate");FD original0(sockets[0]),original1(sockets[1]),parent(fcntl(sockets[0],F_DUPFD_CLOEXEC,3)),child_end(fcntl(sockets[1],F_DUPFD_CLOEXEC,3));ok(parent.n>=3&&child_end.n>=3,"dispatch-gate-duplicate");original0.reset();original1.reset();
    std::vector<std::string> args{argv[0]};for(int i=2;i<argc;++i)args.emplace_back(argv[i]);fflush(nullptr);pid_t child=fork();ok(child>=0,"dispatch-fork");
    if(!child){
        parent.reset();int result=90;
        try{ok(setsid()>=0,"dispatch-child-session",90);FD copy(fcntl(log.n,F_DUPFD_CLOEXEC,3));ok(copy.n>=3,"dispatch-child-log",90);FD null(high(open("/dev/null",O_RDONLY|O_CLOEXEC)));ok(null.n>=3&&dup2(null.n,0)>=0&&dup2(copy.n,1)>=0&&dup2(copy.n,2)>=0,"dispatch-child-stdio",90);log.reset();
            // 先写子进程身份，再通知父进程。此时尚未调用安装/卸载业务入口。
            create(dir.n,prefix+".started","NATIVE_DISPATCH_STARTED_V1\noperation="+action+"\nattempt="+attempt+"\npid="+std::to_string(getpid())+"\nstart="+identity(getpid())+"\n");
            (void)send(child_end.n,"R",1,MSG_NOSIGNAL);child_end.reset();result=call(entry,args);
        }catch(const Error&e){result=e.recovery_failed?90:e.rc;fprintf(stderr,"NATIVE_DISPATCH_CHILD_FAILED step=%s rc=%d errno=%d\n",e.what(),result,e.system_errno);}catch(const std::exception&e){result=90;fprintf(stderr,"NATIVE_DISPATCH_CHILD_FAILED detail=%s\n",e.what());}
        try{create(dir.n,prefix+".exit",std::to_string(result)+"\n");}catch(const std::exception&e){fprintf(stderr,"NATIVE_DISPATCH_EXIT_UNCONFIRMED %s\n",e.what());result=90;}fflush(nullptr);_exit(result);
    }
    child_end.reset();log.reset();pollfd p{parent.n,POLLIN,0};int ready;do{ready=poll(&p,1,5000);}while(ready<0&&errno==EINTR);
    if(ready>0&&(p.revents&POLLIN)){char byte=0;ssize_t n=recv(parent.n,&byte,1,0);if(n==1&&byte=='R'){printf("NATIVE_DISPATCHED_V1 operation=%s attempt=%s\n",action.c_str(),attempt.c_str());return 0;}}
    // 不 kill 或重新派发；子进程可能已经进入事务。主机只能继续读取现场。
    fprintf(stderr,"NATIVE_DISPATCH_UNCONFIRMED operation=%s attempt=%s\n",action.c_str(),attempt.c_str());return 91;
}
} }
extern "C" int cn_dispatch_main(int argc,char**argv){try{return cn::dispatch::run(argc,argv);}catch(const cn::Error&e){fprintf(stderr,"NATIVE_DISPATCH_REFUSED step=%s rc=%d errno=%d\n",e.what(),e.rc,e.system_errno);return e.recovery_failed?90:e.rc;}catch(const std::exception&e){fprintf(stderr,"NATIVE_DISPATCH_REFUSED detail=%s\n",e.what());return 1;}}
