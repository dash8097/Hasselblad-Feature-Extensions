/* 只读固定事务回执；不连接服务、不创建目录、不执行 shell。 */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
namespace native_host_read {
struct FD {int n;explicit FD(int v):n(v){}~FD(){if(n>=0)close(n);}FD(const FD&)=delete;};
struct Record {std::string state="ABSENT",data;};
static bool hex(const std::string&s,size_t n){return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static void need(bool v){if(!v)throw std::runtime_error("invalid fixed transaction arguments");}
static std::string failure(int e){return e==ENOENT?"ABSENT":e==ELOOP||e==ENOTDIR?"TYPE_REJECTED":"READ_ERROR";}
static int directory(const std::string&path){int fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd<0)return -1;std::istringstream parts(path.substr(1));std::string part;while(std::getline(parts,part,'/')){int next=openat(fd,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);int e=errno;close(fd);if(next<0){errno=e;return -1;}fd=next;}return fd;}
static Record read(int parent,const std::string&name){
 FD file(openat(parent,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));if(file.n<0)return {failure(errno),{}};
 struct stat before{},after{},path{};if(fstat(file.n,&before))return {"READ_ERROR",{}};
 if(!S_ISREG(before.st_mode)||before.st_nlink!=1)return {"TYPE_REJECTED",{}};
 if(before.st_size<0||before.st_size>16384)return {"TOO_LARGE",{}};
 std::string data;char buffer[4096];for(;;){auto n=::read(file.n,buffer,sizeof buffer);if(n<0&&errno==EINTR)continue;if(n<0)return {"READ_ERROR",{}};if(!n)break;if(data.size()+size_t(n)>16384)return {"TOO_LARGE",{}};data.append(buffer,size_t(n));}
 if(fstat(file.n,&after)||fstatat(parent,name.c_str(),&path,AT_SYMLINK_NOFOLLOW))return {"UNSTABLE",{}};
 if(!S_ISREG(path.st_mode)||before.st_dev!=path.st_dev||before.st_ino!=path.st_ino||after.st_nlink!=1||before.st_size!=after.st_size||data.size()!=size_t(after.st_size)||before.st_mtim.tv_sec!=after.st_mtim.tv_sec||before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec)return {"UNSTABLE",{}};
 return {"PRESENT",data};
}
static std::string b64(const std::string&s){const char*t="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;out.reserve((s.size()+2)/3*4);for(size_t i=0;i<s.size();i+=3){unsigned a=static_cast<unsigned char>(s[i]),b=i+1<s.size()?static_cast<unsigned char>(s[i+1]):0,c=i+2<s.size()?static_cast<unsigned char>(s[i+2]):0;out+=t[a>>2];out+=t[((a&3)<<4)|(b>>4)];out+=i+1<s.size()?t[((b&15)<<2)|(c>>6)]:'=';out+=i+2<s.size()?t[c&63]:'=';}return out;}
static std::string collect(int argc,char**argv){
 need(argc==5&&std::string(argv[1])=="transaction-read");std::string op=argv[2],stage=argv[3],attempt=argv[4];need(hex(attempt,16));
 if(op=="install-stock"||(op=="payload-unpack"&&stage=="/data/local/tmp/x2d-rs1"))need(stage=="/data/local/tmp/x2d-rs1");else{bool cleanup=op=="data-cleanup";need(cleanup||op=="uninstall"||op=="retained-cleanup"||op=="payload-unpack");std::string prefix=cleanup?"/data/local/tmp/x2d-tidy-":"/data/local/tmp/x2d-un-";need(stage.rfind(prefix,0)==0&&hex(stage.substr(prefix.size()),8));}
 const std::string prefix="dispatch."+attempt;std::vector<std::pair<std::string,std::string>> paths={{"claim",prefix+".claim"},{"started",prefix+".started"},{"exit",prefix+".exit"},{"final",op=="payload-unpack"?"unpack.final."+attempt:op=="install-stock"?"install.final."+attempt:op=="retained-cleanup"?"cleanup.final":"final"}};
 FD parent(directory(stage));const std::string absent=parent.n<0?failure(errno):"";
 std::string out="{\"protocol\":\"NATIVE_TRANSACTION_READ_V1\",\"operation\":\""+op+"\",\"stage\":\""+stage+"\",\"attempt\":\""+attempt+"\",\"files\":{";
 bool first=true;for(const auto&entry:paths){Record r=parent.n<0?Record{absent,{}}:read(parent.n,entry.second);if(!first)out+=',';first=false;out+='"'+entry.first+"\":{\"state\":\""+r.state+"\",\"data\":\""+b64(r.data)+"\"}";}return out+"}}\n";
}
}
extern "C" int cn_transaction_read_main(int argc,char**argv){try{auto out=native_host_read::collect(argc,argv);if(fwrite(out.data(),1,out.size(),stdout)!=out.size()||fflush(stdout))return 1;return 0;}catch(const std::exception&e){fprintf(stderr,"NATIVE_TRANSACTION_READ_REFUSED %s\n",e.what());return 2;}}
