/* 固定 gzip/USTAR 白名单解包；不调用 tar、不解释脚本、不改变相机或 /system。 */
#include "native_common.hpp"
#include "native_unpack.hpp"
#include "native_unpack_tables.hpp"
#include "receipt_sha256.h"
#ifndef CN_UNPACK_DECODER_SHA256
#error Fixed firmware decoder digest is required
#endif
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>
#include <dirent.h>
extern "C" int cn_signal_pending(void);
namespace cn { namespace unpack {
struct FD {int n;explicit FD(int v=-1):n(v){}~FD(){if(n>=0)close(n);}FD(const FD&)=delete;void reset(){if(n>=0)close(n);n=-1;}};
static void ok(bool v,const char*s,int rc=1){if(!v)throw Error(s,rc,false,errno);}
static bool hex(const std::string&s,size_t n){return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static bool name(const std::string&s){return !s.empty()&&s.size()<100&&s!="."&&s!=".."&&s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")==std::string::npos;}
static int high(int fd){if(fd<0||fd>=3)return fd;int next=fcntl(fd,F_DUPFD_CLOEXEC,3),e=errno;close(fd);if(next<0)throw Error("unpack-fd-duplicate",1,false,e);return next;}
static int pathdir(const std::string&p){int fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);ok(fd>=0,"unpack-root");try{std::istringstream parts(p.substr(1));std::string part;while(std::getline(parts,part,'/')){require(name(part),"unpack-directory-component",2);int next=openat(fd,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);ok(next>=0,"unpack-directory");close(fd);fd=next;}return high(fd);}catch(...){close(fd);throw;}}
static bool present(int dir,const std::string&n){struct stat st;if(!fstatat(dir,n.c_str(),&st,AT_SYMLINK_NOFOLLOW))return true;ok(errno==ENOENT,"unpack-entry-state");return false;}
static void write_all(int fd,const char*p,size_t size){while(size){cancelled();ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;ok(n>0,"unpack-write");p+=n;size-=size_t(n);}}
static std::string finish_hash(ReceiptSha&h){unsigned char bytes[32];rsha_final(&h,bytes);std::string out;for(auto b:bytes){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;}
static std::string hash_fd(int fd){ReceiptSha h;rsha_init(&h);char buffer[65536];for(;;){cancelled();ssize_t n=::read(fd,buffer,sizeof buffer);if(n<0&&errno==EINTR)continue;ok(n>=0,"unpack-archive-read");if(!n)break;rsha_update(&h,reinterpret_cast<unsigned char*>(buffer),unsigned(n));}ok(lseek(fd,0,SEEK_SET)==0,"unpack-archive-rewind");return finish_hash(h);}
static void same_file(int fd,const struct stat&before){struct stat now;ok(!fstat(fd,&now),"unpack-archive-stat");require(now.st_dev==before.st_dev&&now.st_ino==before.st_ino&&now.st_nlink==1&&now.st_size==before.st_size&&now.st_mtim.tv_sec==before.st_mtim.tv_sec&&now.st_mtim.tv_nsec==before.st_mtim.tv_nsec&&now.st_ctim.tv_sec==before.st_ctim.tv_sec&&now.st_ctim.tv_nsec==before.st_ctim.tv_nsec,"unpack-archive-changed");}
struct Decoder {
 pid_t pid=-1;int output=-1,result=-1;uint64_t deadline=0;
 ~Decoder(){if(output>=0)close(output);if(pid>0){try{stop();}catch(...){}}}
 bool poll(){if(pid<0)return true;int status=0;pid_t found;do{found=waitpid(pid,&status,WNOHANG);}while(found<0&&errno==EINTR);if(!found)return false;ok(found==pid,"unpack-decoder-wait",90);pid=-1;result=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);return true;}
 void stop(){if(pid<0)return;Recovery recovery;if(poll())return;ok(!kill(pid,SIGTERM)||errno==ESRCH,"unpack-decoder-term",90);for(unsigned i=0;i<100&&!poll();i++)usleep(20000);if(pid>0){ok(!kill(pid,SIGKILL)||errno==ESRCH,"unpack-decoder-kill",90);for(unsigned i=0;i<100&&!poll();i++)usleep(20000);}require(pid<0,"unpack-decoder-unreaped",90);}
 void start(int archive,int error_log){
  FD decoder(high(open("/system/xbin/busybox",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC)));ok(decoder.n>=0,"unpack-decoder-open");struct stat busybox;ok(!fstat(decoder.n,&busybox),"unpack-decoder-stat");require(S_ISREG(busybox.st_mode)&&busybox.st_nlink==1&&(busybox.st_mode&0111),"unpack-decoder-type");require(hash_fd(decoder.n)==CN_UNPACK_DECODER_SHA256,"unpack-decoder-digest");same_file(decoder.n,busybox);struct stat named;ok(!lstat("/system/xbin/busybox",&named),"unpack-decoder-name");require(named.st_dev==busybox.st_dev&&named.st_ino==busybox.st_ino,"unpack-decoder-replaced");
  int pipes[2];ok(!pipe2(pipes,O_CLOEXEC),"unpack-decoder-pipe");FD input(high(pipes[0])),writer(high(pipes[1]));pid=fork();ok(pid>=0,"unpack-decoder-fork");
  if(!pid){struct stat now;if(lstat("/system/xbin/busybox",&now)||now.st_dev!=busybox.st_dev||now.st_ino!=busybox.st_ino)_exit(126);int in=fcntl(archive,F_DUPFD_CLOEXEC,3),out=fcntl(writer.n,F_DUPFD_CLOEXEC,3),err=fcntl(error_log,F_DUPFD_CLOEXEC,3);if(in<3||out<3||err<3||dup2(in,0)<0||dup2(out,1)<0||dup2(err,2)<0)_exit(126);close(in);close(out);close(err);char*p=const_cast<char*>("/system/xbin/busybox");char*argv[]={p,const_cast<char*>("gzip"),const_cast<char*>("-dc"),nullptr};char*env[]={const_cast<char*>("PATH=/system/bin:/system/xbin"),nullptr};signal(SIGTERM,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGHUP,SIG_DFL);execve(p,argv,env);_exit(127);}
  output=input.n;input.n=-1;writer.reset();int flags=fcntl(output,F_GETFL);ok(flags>=0&&!fcntl(output,F_SETFL,flags|O_NONBLOCK),"unpack-decoder-nonblock");deadline=monotonic_ms()+300000;
 }
 size_t read(char*buffer,size_t size){for(;;){cancelled();require(monotonic_ms()<deadline,"unpack-decode-timeout",124);ssize_t n=::read(output,buffer,size);if(n>=0)return size_t(n);if(errno==EINTR)continue;ok(errno==EAGAIN||errno==EWOULDBLOCK,"unpack-decode-read");pollfd wait{output,POLLIN,0};int rc=::poll(&wait,1,100);ok(rc>=0||errno==EINTR,"unpack-decode-poll");}}
 void exact(char*buffer,size_t size){while(size){size_t n=read(buffer,size);require(n>0,"unpack-truncated-archive");buffer+=n;size-=n;}}
 void finish(){for(unsigned i=0;!poll();i++){require(i<100,"unpack-decoder-exit-timeout",124);sleep_ms(20);}require(result==0,"unpack-decoder-failed",result?result:1);}
};
static uint64_t octal(const char*p,size_t size){uint64_t value=0;bool ended=false,seen=false;for(size_t i=0;i<size;i++){unsigned char c=p[i];if(c==0||c==' '){ended=true;continue;}require(!ended&&c>='0'&&c<='7'&&value<=(UINT64_MAX-7)/8,"unpack-octal");seen=true;value=value*8+c-'0';}require(seen,"unpack-empty-octal");return value;}
static bool zeros(const char*p,size_t size){for(size_t i=0;i<size;i++)if(p[i])return false;return true;}
static std::string tar_name(const char*block){const void*end=memchr(block,0,100);require(end!=nullptr,"unpack-name-termination");auto length=static_cast<const char*>(end)-block;require(zeros(block+length,100-size_t(length)),"unpack-name-padding");std::string s(block,size_t(length));require(name(s),"unpack-name");return s;}
struct Created {const File*spec;struct stat identity{};bool created=false;};
static void verify_directory(int held,int parent,const std::string&name,const struct stat&expected,const std::vector<Created>&files,bool after_commit){
 const auto fail=[after_commit](const char*step){throw Error(step,1,after_commit,0);};struct stat descriptor{},named{};
 if(fstat(held,&descriptor)||fstatat(parent,name.c_str(),&named,AT_SYMLINK_NOFOLLOW)||!S_ISDIR(named.st_mode)||named.st_dev!=expected.st_dev||named.st_ino!=expected.st_ino||descriptor.st_dev!=expected.st_dev||descriptor.st_ino!=expected.st_ino)fail(after_commit?"unpack-committed-directory-replaced":"unpack-temp-directory-replaced");
 FD iterator(openat(held,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ok(iterator.n>=0,"unpack-verify-directory-open");DIR*stream=fdopendir(iterator.n);ok(stream!=nullptr,"unpack-verify-directory-stream");iterator.n=-1;size_t seen=0;
 try{for(;;){errno=0;auto*entry=readdir(stream);if(!entry){ok(errno==0,"unpack-verify-directory-read");break;}std::string child=entry->d_name;if(child=="."||child=="..")continue;bool known=false;for(const auto&f:files)if(child==f.spec->name)known=true;if(!known)fail("unpack-foreign-directory-entry");seen++;}}catch(...){closedir(stream);throw;}closedir(stream);
 if(seen!=files.size())fail("unpack-directory-entry-count");
 for(const auto&file:files){FD fd(openat(held,file.spec->name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));ok(fd.n>=0,"unpack-verify-file-open");struct stat st{},path{};if(!file.created||fstat(fd.n,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||st.st_dev!=file.identity.st_dev||st.st_ino!=file.identity.st_ino||uint64_t(st.st_size)!=file.spec->size||(st.st_mode&07777)!=file.spec->mode)fail("unpack-verified-file-replaced");if(hash_fd(fd.n)!=file.spec->sha256)fail("unpack-verified-file-digest");same_file(fd.n,st);if(fstatat(held,file.spec->name,&path,AT_SYMLINK_NOFOLLOW)||!S_ISREG(path.st_mode)||path.st_dev!=st.st_dev||path.st_ino!=st.st_ino)fail("unpack-verified-file-name-changed");}
 if(fstatat(parent,name.c_str(),&named,AT_SYMLINK_NOFOLLOW)||!S_ISDIR(named.st_mode)||named.st_dev!=expected.st_dev||named.st_ino!=expected.st_ino)fail("unpack-directory-changed-during-verification");
}
static void extract(Decoder&decoder,int directory,std::vector<Created>&files){
 uint64_t consumed=0;char block[512];unsigned count=0;
 for(;;){decoder.exact(block,sizeof block);consumed+=512;require(consumed<=uint64_t(1024)*1024*1024,"unpack-stream-limit");if(zeros(block,512)){decoder.exact(block,512);require(zeros(block,512),"unpack-end-marker");break;}
  require(!memcmp(block+257,"ustar\0",6)&&!memcmp(block+263,"00",2),"unpack-ustar-only");require((block[156]==0||block[156]=='0')&&zeros(block+157,100)&&zeros(block+345,167),"unpack-regular-flat-only");
  uint64_t checksum=0;for(size_t i=0;i<512;i++)checksum+=(i>=148&&i<156)?32:static_cast<unsigned char>(block[i]);require(checksum==octal(block+148,8),"unpack-header-checksum");auto n=tar_name(block);Created*entry=nullptr;for(auto&f:files)if(n==f.spec->name)entry=&f;require(entry&&!entry->created,"unpack-unlisted-or-duplicate");
  auto size=octal(block+124,12);require(size==entry->spec->size&&octal(block+100,8)==entry->spec->mode&&octal(block+108,8)==0&&octal(block+116,8)==0,"unpack-entry-metadata");
  FD output(openat(directory,n.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));ok(output.n>=0,"unpack-output-exclusive");ok(!fstat(output.n,&entry->identity),"unpack-output-identity");entry->created=true;ReceiptSha h;rsha_init(&h);char buffer[65536];uint64_t left=size;
  while(left){size_t part=size_t(left>sizeof buffer?sizeof buffer:left);decoder.exact(buffer,part);write_all(output.n,buffer,part);rsha_update(&h,reinterpret_cast<unsigned char*>(buffer),unsigned(part));left-=part;consumed+=part;}
  require(finish_hash(h)==entry->spec->sha256,"unpack-entry-digest");ok(!fchmod(output.n,entry->spec->mode)&&!fsync(output.n),"unpack-output-sync");size_t padding=size_t((512-size%512)%512);if(padding){decoder.exact(block,padding);require(zeros(block,padding),"unpack-data-padding");consumed+=padding;}count++;
 }
 require(count==files.size(),"unpack-missing-entry");size_t padding=0;for(;;){auto n=decoder.read(block,sizeof block);if(!n)break;padding+=n;require(padding<=10240&&zeros(block,n),"unpack-trailing-data");}decoder.finish();ok(!fsync(directory),"unpack-directory-sync");
}
static void final_receipt(int stage,const std::string&attempt,const std::string&text){std::string target="unpack.final."+attempt,next=target+".next";FD output(openat(stage,next.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));ok(output.n>=0,"unpack-final-create",90);write_all(output.n,text.data(),text.size());ok(!fsync(output.n),"unpack-final-sync",90);ok(!syscall(SYS_renameat2,stage,next.c_str(),stage,target.c_str(),1),"unpack-final-rename",90);ok(!fsync(stage),"unpack-final-directory",90);}
static int run(const std::string&kind,const std::string&stage,const std::string&attempt){
 require(hex(attempt,16),"unpack-attempt",2);const Bundle*b=nullptr;if(kind=="install-stock"){require(stage=="/data/local/tmp/x2d-rs1","unpack-stage",2);b=&install_bundle;}else{require(kind=="uninstall"&&stage.rfind("/data/local/tmp/x2d-un-",0)==0&&hex(stage.substr(23),8),"unpack-stage",2);b=&uninstall_bundle;}
 require(name(b->archive_name)&&hex(b->sha256,64)&&b->compressed_size>0&&b->compressed_size<=512ull*1024*1024&&b->count>0&&b->count<=512,"unpack-table",2);
 std::vector<Created> files;files.reserve(b->count);uint64_t total=0;for(size_t i=0;i<b->count;i++){const auto&f=b->files[i];require(name(f.name)&&hex(f.sha256,64)&&f.size<=256ull*1024*1024&&(f.mode==0644||f.mode==0755),"unpack-file-table",2);for(const auto&prior:files)require(std::string(prior.spec->name)!=f.name,"unpack-table-duplicate",2);total+=f.size;require(total<=1024ull*1024*1024,"unpack-total-size",2);files.push_back({&f,{ },false});}
 TransactionGuard transaction;FD directory(pathdir(stage));require(!present(directory.n,"payload")&&!present(directory.n,"unpack.final."+attempt)&&!present(directory.n,"started")&&!present(directory.n,"result")&&!present(directory.n,"backup"),"unpack-stage-not-fresh",3);
 FD archive(high(openat(directory.n,b->archive_name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK)));ok(archive.n>=0,"unpack-archive-open");struct stat source{};ok(!fstat(archive.n,&source),"unpack-archive-stat");require(S_ISREG(source.st_mode)&&source.st_nlink==1&&uint64_t(source.st_size)==b->compressed_size,"unpack-archive-type-size");require(hash_fd(archive.n)==b->sha256,"unpack-archive-digest");same_file(archive.n,source);
 struct statvfs space;ok(!fstatvfs(directory.n,&space),"unpack-space");uint64_t unit=space.f_frsize?space.f_frsize:space.f_bsize;require(unit>0&&uint64_t(space.f_bavail)>=total/unit+(total%unit!=0)+b->count+4,"unpack-space-low",28);
 std::string temp=".payload-"+attempt;struct stat temp_id{};bool created=false,known=false,committed=false,payload_ready=false;int primary=0,recovery=0,primary_errno=0;std::string step="none";FD payload;Decoder decoder;
 try{ok(!mkdirat(directory.n,temp.c_str(),0700),"unpack-temp-exclusive");created=true;ok(!fstatat(directory.n,temp.c_str(),&temp_id,AT_SYMLINK_NOFOLLOW),"unpack-temp-stat");known=true;payload.n=high(openat(directory.n,temp.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ok(payload.n>=0,"unpack-temp-open");FD log(high(openat(directory.n,("unpack.decoder."+attempt+".log").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600)));ok(log.n>=0,"unpack-decoder-log");decoder.start(archive.n,log.n);extract(decoder,payload.n,files);same_file(archive.n,source);verify_directory(payload.n,directory.n,temp,temp_id,files,false);cancelled();ok(!syscall(SYS_renameat2,directory.n,temp.c_str(),directory.n,"payload",1),"unpack-commit");committed=true;verify_directory(payload.n,directory.n,"payload",temp_id,files,true);ok(!fsync(directory.n),"unpack-commit-sync");payload_ready=true;}
 catch(const Error&e){primary=e.rc;primary_errno=e.system_errno;step=e.what();if(e.recovery_failed||committed)recovery=90;}catch(const std::exception&e){primary=1;step="native-exception";if(committed)recovery=90;fprintf(stderr,"UNPACK_EXCEPTION %s\n",e.what());}
 if(!primary&&stopping()){primary=143;step="cancelled";}
 {Recovery scope;try{decoder.stop();}catch(const std::exception&e){recovery=90;fprintf(stderr,"UNPACK_DECODER_RESTORE %s\n",e.what());}
  if(created&&!committed){for(auto&f:files)if(f.created)try{struct stat now;ok(payload.n>=0&&!fstatat(payload.n,f.spec->name,&now,AT_SYMLINK_NOFOLLOW),"unpack-cleanup-state",90);require(now.st_dev==f.identity.st_dev&&now.st_ino==f.identity.st_ino,"unpack-cleanup-foreign",90);ok(!unlinkat(payload.n,f.spec->name,0),"unpack-cleanup-file",90);}catch(const std::exception&e){recovery=90;fprintf(stderr,"UNPACK_FILE_RESTORE %s\n",e.what());}
   try{struct stat now;require(known&&!fstatat(directory.n,temp.c_str(),&now,AT_SYMLINK_NOFOLLOW)&&now.st_dev==temp_id.st_dev&&now.st_ino==temp_id.st_ino,"unpack-cleanup-directory-identity",90);ok(!unlinkat(directory.n,temp.c_str(),AT_REMOVEDIR)&&!fsync(directory.n),"unpack-cleanup-directory",90);}catch(const std::exception&e){recovery=90;fprintf(stderr,"UNPACK_DIRECTORY_RESTORE %s\n",e.what());}}
  if(!primary&&cn_signal_pending()){primary=143;step="cancelled";}
  try{final_receipt(directory.n,attempt,"NATIVE_TRANSACTION_V1\noperation=payload-unpack\nkind="+kind+"\nstage="+stage+"\nattempt="+attempt+"\nmanifest="+b->sha256+"\nprimary_step="+step+"\nprimary_errno="+std::to_string(primary_errno)+"\nprimary_rc="+std::to_string(primary)+"\nrestore_rc="+std::to_string(recovery)+"\nfinal_rc="+std::to_string(recovery?recovery:primary)+"\npayload_ready="+(payload_ready?"1\n":"0\n"));}catch(const std::exception&e){recovery=90;fprintf(stderr,"UNPACK_FINAL_UNCONFIRMED %s\n",e.what());}
 }fprintf(stderr,"UNPACK_FINISH primary_rc=%d restore_rc=%d payload_ready=%d\n",primary,recovery,payload_ready);return recovery?recovery:primary;
}
} }
extern "C" int cn_unpack_main(int argc,char**argv){try{cn::require(argc==5&&std::string(argv[1])=="payload-unpack","unpack-arguments",2);return cn::unpack::run(argv[2],argv[3],argv[4]);}catch(const cn::Error&e){fprintf(stderr,"UNPACK_REFUSED step=%s rc=%d errno=%d\n",e.what(),e.rc,e.system_errno);return e.recovery_failed?90:e.rc;}catch(const std::exception&e){fprintf(stderr,"UNPACK_REFUSED detail=%s\n",e.what());return 1;}}
