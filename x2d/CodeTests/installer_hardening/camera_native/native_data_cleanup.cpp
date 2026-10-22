/* X2D 4.2.0 固定项目清理；只操作可信目录 FD，旧脚本仅作为历史归属证据读取。 */
#include "native_common.hpp"
#include "legacy_cleanup_stages.hpp"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <memory>
#include <signal.h>
#include <set>
#include <sstream>
extern "C" int cn_signal_pending(void);
namespace cn { namespace tidy {
static void ok(bool yes,const char*step,int rc=1){if(!yes)throw Error(step,rc,false,errno);}
struct FD {int n;explicit FD(int v):n(v){ok(n>=0,"tidy-open");}~FD(){if(n>=0)close(n);}FD(const FD&)=delete;};
static bool hex(const std::string&s,size_t n){return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static std::string folded(std::string s){for(auto&c:s)if(c>='A'&&c<='Z')c+=32;return s;}
static bool under(const std::string&p,const std::string&d){return p==d||(p.size()>d.size()&&p.compare(0,d.size(),d)==0&&p[d.size()]=='/');}
static bool same(const struct stat&a,const struct stat&b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino;}
static bool state(int dir,const std::string&name,struct stat&st){if(!fstatat(dir,name.c_str(),&st,AT_SYMLINK_NOFOLLOW))return true;ok(errno==ENOENT,"tidy-stat");return false;}
static int pathdir(const std::string&path){
    require(!path.empty()&&path[0]=='/',"tidy-absolute-path",2);int dir=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);ok(dir>=0,"tidy-root");
    try{size_t start=1;while(start<path.size()){size_t end=path.find('/',start);if(end==std::string::npos)end=path.size();auto part=path.substr(start,end-start);require(!part.empty()&&part!="."&&part!="..","tidy-path-part",2);int child=openat(dir,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);ok(child>=0,"tidy-parent-directory");close(dir);dir=child;start=end+1;}return dir;}catch(...){close(dir);throw;}
}
static std::vector<std::string> entries(int dir){
    int copy=openat(dir,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);ok(copy>=0,"tidy-list-open");DIR*d=fdopendir(copy);if(!d){close(copy);throw Error("tidy-list-dir",1,false,errno);}std::vector<std::string> names;
    try{for(;;){errno=0;auto*entry=readdir(d);if(!entry){ok(!errno,"tidy-list-read");break;}std::string name=entry->d_name;if(name=="."||name=="..")continue;require(names.size()<100000,"tidy-directory-limit");names.push_back(name);}closedir(d);}catch(...){closedir(d);throw;}std::sort(names.begin(),names.end());return names;
}
static std::string content(int dir,const std::string&name,size_t limit=65536){
    FD file(openat(dir,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));struct stat st;ok(!fstat(file.n,&st),"tidy-read-stat");require(S_ISREG(st.st_mode)&&st.st_nlink==1,"tidy-read-type");std::string out;char b[4096];
    for(;;){ssize_t n=::read(file.n,b,sizeof b);if(n<0&&errno==EINTR)continue;ok(n>=0,"tidy-read");if(!n)break;require(out.size()+size_t(n)<=limit,"tidy-read-limit");out.append(b,size_t(n));}return out;
}
static void remove_owned(int dir,const std::string&name,const struct stat&owner,int flags=0){struct stat st;require(state(dir,name,st)&&same(st,owner),"tidy-object-changed",90);ok(!unlinkat(dir,name.c_str(),flags),"tidy-unlink",90);}
static void receipt(int dir,const std::string&name,const std::string&text){
    const auto next=name+".native-next";FD file(openat(dir,next.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));struct stat owner;ok(!fstat(file.n,&owner),"tidy-receipt-stat");bool renamed=false;
    try{size_t at=0;while(at<text.size()){ssize_t n=write(file.n,text.data()+at,text.size()-at);if(n<0&&errno==EINTR)continue;ok(n>0,"tidy-receipt-write");at+=size_t(n);}ok(!fsync(file.n),"tidy-receipt-fsync");struct stat target;if(state(dir,name,target))require(S_ISREG(target.st_mode)&&target.st_nlink==1,"tidy-receipt-conflict");ok(!renameat(dir,next.c_str(),dir,name.c_str()),"tidy-receipt-rename");renamed=true;ok(!fsync(dir),"tidy-receipt-directory-fsync");}
    catch(const std::exception&e){if(!renamed)try{remove_owned(dir,next,owner);}catch(...){const auto*p=dynamic_cast<const Error*>(&e);throw Error(e.what(),p?p->rc:1,true,p?p->system_errno:0);}throw;}
}
struct Lock {std::string name;int parent;struct stat owner{};bool owned=false;Lock(int p,std::string s):name(std::move(s)),parent(p){}bool acquire(){if(mkdirat(parent,name.c_str(),0700)){if(errno==EEXIST)return false;ok(false,"tidy-lock-create");}owned=true;ok(!fstatat(parent,name.c_str(),&owner,AT_SYMLINK_NOFOLLOW),"tidy-lock-stat",90);return true;}void release(){if(owned){remove_owned(parent,name,owner,AT_REMOVEDIR);owned=false;}}};
struct Refs {
    bool incomplete=false;
    std::vector<std::string> paths;std::set<std::pair<dev_t,ino_t>> nodes;
    void add(const std::string&p){if(!p.empty()&&p[0]=='/')paths.push_back(folded(p));}
    void inode(const struct stat&st){nodes.emplace(st.st_dev,st.st_ino);}
    bool used(const std::string&path,const struct stat&st,bool kernel_proof=false)const{if(incomplete&&!kernel_proof)return true;if(nodes.count({st.st_dev,st.st_ino}))return true;const auto root=folded(path);for(const auto&p:paths)if(under(p,root))return true;return false;}
};
static std::string unescape(const std::string&s){std::string out;for(size_t i=0;i<s.size();++i){if(s[i]=='\\'){require(i+3<s.size()&&s[i+1]>='0'&&s[i+1]<='7'&&s[i+2]>='0'&&s[i+2]<='7'&&s[i+3]>='0'&&s[i+3]<='7',"tidy-mount-escape");char c=char((s[i+1]-'0')*64+(s[i+2]-'0')*8+s[i+3]-'0');require(c!=0,"tidy-mount-nul");out+=c;i+=3;}else out+=s[i];}return out;}
static Refs references(bool kernel_proof=false){
    Refs refs;FD proc(pathdir("/proc"));for(const auto&pid:entries(proc.n)){
        if(!digits(pid)||pid==std::to_string(getpid()))continue;int raw=openat(proc.n,pid.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(raw<0&&errno==ENOENT)continue;FD process(raw);
        auto alive=[&](){struct stat st;return state(proc.n,pid,st);};
        std::string stat;try{stat=content(process.n,"stat");}catch(...){if(!alive())continue;throw;}
        auto close=stat.rfind(')');require(close!=std::string::npos,"tidy-process-stat");std::istringstream fields(stat.substr(close+1));std::string field,status,flags;for(int i=3;i<=9;i++){require(bool(fields>>field),"tidy-process-fields");if(i==3)status=field;if(i==9)flags=field;}require(digits(flags)&&flags.size()<20,"tidy-process-flags");unsigned long long bits=std::stoull(flags);if(status=="Z"||(bits&0x00200000ull))continue;
        auto link=[&](int parent,const std::string&name,bool transient){
            char b[8192];ssize_t n=readlinkat(parent,name.c_str(),b,sizeof b);
            if(n<0){const int error=errno;if(error==ENOENT&&(transient||!alive()))return;throw Error("tidy-reference-link:"+pid+":"+name,1,false,error);}
            require(size_t(n)<sizeof b,"tidy-reference-truncated");const std::string target(b,size_t(n));refs.add(target);
            // /proc fd 的 socket/pipe 是内核对象，不是可删除目录中的文件。
            // 原厂域允许 readlink，但可拒绝对 init 的 socket inode 做 stat。
            // 只识别严格的内核对象语法；文件路径和其他类型仍必须核对 inode。
            for(const char*prefix:{"socket:[","pipe:["}){
                const std::string tag=prefix;
                if(transient&&target.rfind(tag,0)==0&&target.back()==']'&&digits(target.substr(tag.size(),target.size()-tag.size()-1)))return;
            }
            struct stat st;if(!fstatat(parent,name.c_str(),&st,0))refs.inode(st);
            else {const int error=errno;if(error!=ENOENT&&alive())throw Error("tidy-reference-stat:"+pid+":"+name,1,false,error);}
        };
        try{
            link(process.n,"cwd",false);link(process.n,"exe",false);FD fds(openat(process.n,"fd",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));for(const auto&fd:entries(fds.n)){if(digits(fd))link(fds.n,fd,true);}
            auto cmd=content(process.n,"cmdline",1024*1024);size_t at=0;while(at<cmd.size()){auto end=cmd.find('\0',at);if(end==std::string::npos)end=cmd.size();refs.add(cmd.substr(at,end-at));at=end+1;}
            std::istringstream maps(content(process.n,"maps",16*1024*1024));std::string line;while(std::getline(maps,line)){std::istringstream row(line);std::string range,perms,offset,device,ino;require(bool(row>>range>>perms>>offset>>device>>ino),"tidy-map-row");unsigned major_part,minor_part;char extra;require(sscanf(device.c_str(),"%x:%x%c",&major_part,&minor_part,&extra)==2&&digits(ino)&&ino.size()<21,"tidy-map-identity");auto number=std::stoull(ino);if(number)refs.nodes.emplace(makedev(major_part,minor_part),ino_t(number));std::string path;std::getline(row,path);auto first=path.find_first_not_of(' ');if(first!=std::string::npos)refs.add(path.substr(first));}
        }catch(const Error&e){if(!alive())continue;if(kernel_proof&&(e.system_errno==EACCES||e.system_errno==EPERM)){refs.incomplete=true;continue;}throw;}
    }
    std::istringstream mounts(read("/proc/self/mounts",1024*1024));std::string line;while(std::getline(mounts,line)){std::istringstream row(line);std::string source,target;require(bool(row>>source>>target),"tidy-mount-row");auto path=unescape(target);refs.add(path);struct stat st;if(!stat(path.c_str(),&st))refs.inode(st);else ok(false,"tidy-mount-target");}return refs;
}
struct Node {std::string name,path;struct stat st{};std::vector<Node> children;};
static Node inspect(int dir,const std::string&name,const std::string&path,dev_t device,size_t&count,unsigned depth=0){
    cancelled();require(depth<64&&++count<100000,"tidy-tree-limit");Node node;node.name=name;node.path=path;require(state(dir,name,node.st),"tidy-tree-vanished");require(node.st.st_dev==device,"tidy-tree-cross-device");
    if(S_ISDIR(node.st.st_mode)){FD child(openat(dir,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat st;ok(!fstat(child.n,&st),"tidy-tree-stat");require(same(st,node.st),"tidy-tree-raced");for(const auto&part:entries(child.n))node.children.push_back(inspect(child.n,part,path+"/"+part,device,count,depth+1));}
    else require(S_ISREG(node.st.st_mode)&&node.st.st_nlink==1,"tidy-tree-link-or-special");return node;
}
static bool referenced(const Node&node,const Refs&refs){if(refs.used(node.path,node.st,true))return true;for(const auto&child:node.children)if(referenced(child,refs))return true;return false;}
static volatile sig_atomic_t lease_break=0;
static void on_lease_break(int){lease_break=1;}
// 不以进程权限拒绝推定空闲。内核租约覆盖每个普通文件；失败就保留整棵树。
// 目录不支持租约，只删除明确归属的空目录；不宣称证明不可读进程没有 cwd 引用。
struct Leases {
    std::vector<int> files;struct sigaction previous{};bool installed=false;
    Leases(){struct sigaction action{};action.sa_handler=on_lease_break;sigemptyset(&action.sa_mask);lease_break=0;ok(!sigaction(SIGIO,&action,&previous),"tidy-lease-signal");installed=true;}
    ~Leases(){for(int fd:files){fcntl(fd,F_SETLEASE,F_UNLCK);close(fd);}if(installed)sigaction(SIGIO,&previous,nullptr);}
    void check()const{require(!lease_break,"tidy-lease-break");for(int fd:files)require(fcntl(fd,F_GETLEASE)==F_WRLCK,"tidy-lease-lost");}
    bool acquire(int parent,const Node&node){
        if(S_ISDIR(node.st.st_mode)){FD dir(openat(parent,node.name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat st;ok(!fstat(dir.n,&st),"tidy-lease-dir-stat");require(same(st,node.st),"tidy-lease-dir-race");for(const auto&child:node.children)if(!acquire(dir.n,child))return false;return true;}
        FD file(openat(parent,node.name.c_str(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC));struct stat st;ok(!fstat(file.n,&st),"tidy-lease-stat");require(same(st,node.st)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"tidy-lease-file-race");
        if(fcntl(file.n,F_SETLEASE,F_WRLCK)){if(errno==EAGAIN||errno==EACCES||errno==EPERM||errno==EINVAL||errno==ENOSYS||errno==ENOLCK)return false;ok(false,"tidy-lease-acquire");}
        files.push_back(file.n);file.n=-1;check();return true;
    }
};
static void erase(int parent,const Node&node,const Leases&leases){
    leases.check();
    cancelled();struct stat st;require(state(parent,node.name,st)&&same(st,node.st)&&st.st_mode==node.st.st_mode,"tidy-delete-identity");
    if(S_ISDIR(st.st_mode)){{FD child(openat(parent,node.name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ok(!fstat(child.n,&st),"tidy-delete-stat");require(same(st,node.st),"tidy-delete-race");auto names=entries(child.n);require(names.size()==node.children.size(),"tidy-delete-tree-changed");for(size_t i=0;i<names.size();++i)require(names[i]==node.children[i].name,"tidy-delete-entries-changed");for(const auto&part:node.children)erase(child.n,part,leases);ok(!fsync(child.n),"tidy-delete-directory-sync");}remove_owned(parent,node.name,node.st,AT_REMOVEDIR);}
    else {require(st.st_nlink==1&&st.st_size==node.st.st_size&&st.st_mtim.tv_sec==node.st.st_mtim.tv_sec&&st.st_mtim.tv_nsec==node.st.st_mtim.tv_nsec,"tidy-delete-file-changed");remove_owned(parent,node.name,node.st);}ok(!fsync(parent),"tidy-delete-parent-sync");
}
static bool safe_remove(int parent,const std::string&name,const std::string&path,bool directory=true){
    struct stat st;if(!state(parent,name,st))return false;if(directory?!S_ISDIR(st.st_mode):!S_ISREG(st.st_mode))return false;size_t count=0;Node tree;
    try{tree=inspect(parent,name,path,st.st_dev,count);}catch(const Error&e){if(e.system_errno==EACCES||e.system_errno==EPERM||std::string(e.what())=="tidy-tree-link-or-special"||std::string(e.what())=="tidy-tree-cross-device")return false;throw;}
    // 每一棵树删除前重新取引用快照；自有任务锁阻止本项目启动新拍摄。
    if(referenced(tree,references(true)))return false;Leases leases;if(!leases.acquire(parent,tree))return false;leases.check();erase(parent,tree,leases);return true;
}
struct LooseFile {const char*name;const char*sha;};
// 历史 inventory-20261007 / components-18bbda4c 与本轮完整摘要回读核对。
static const LooseFile legacy_loose[]={
 {"x2d-dt-66fc547f.bin","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
 {"x2d-memory-r11-host-superseded.b64","a0c85bc5a09b9505c48b31c8dc9174728f7cd66eed352356b455380319bde63a"},
 {"x2d-memory-r5-worker-partial.b64","700dd4eeafa56eced88e7342cbc37c250a387d0a75a607cb4eeb9f06aca2ffe5"}};
static bool stage_name(const std::string&name){
    for(const auto&entry:legacy_stages)if(name==entry.name)return true;
    if(name=="x2d-rs1"||name=="x2d-rs8")return true;
    for(const char*p:{"x2d-un-","x2d-tidy-"}){std::string prefix=p;if(name.rfind(prefix,0)==0&&hex(name.substr(prefix.size()),8))return true;}
    const std::string prefix="x2d-rs1.saved-";return name.rfind(prefix,0)==0&&name.size()>prefix.size()&&name.size()<100&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos;
}
static bool legacy_owned(int parent,const std::string&name){
    bool registered=false;for(const auto&entry:legacy_stages)if(name==entry.name)registered=true;
    if(!registered)return true;
    FD dir(openat(parent,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat st;ok(!fstat(dir.n,&st),"tidy-legacy-stat");if(st.st_uid!=0||(st.st_mode&0022))return false;
    for(const auto&entry:legacy_stages)if(name==entry.name&&*entry.marker){
        if(!state(dir.n,entry.marker,st)||!S_ISREG(st.st_mode)||st.st_nlink!=1)return false;
        if(sha256("/data/local/tmp/"+name+"/"+entry.marker)!=entry.sha)return false;
    }
    return true;
}
static bool ordinary(int dir,const std::string&name){struct stat st;return state(dir,name,st)&&S_ISREG(st.st_mode)&&st.st_nlink==1;}
static bool liveview_temporary(const std::string&name){
    const std::string prefix="x2d-liveview-timeout-",suffix=".next";
    if(name.size()>96||name.rfind(prefix,0)!=0||name.size()<=prefix.size()+suffix.size()||name.compare(name.size()-suffix.size(),suffix.size(),suffix)!=0)return false;
    auto middle=name.substr(prefix.size(),name.size()-prefix.size()-suffix.size());auto dash=middle.find('-');
    return dash!=std::string::npos&&digits(middle.substr(0,dash))&&digits(middle.substr(dash+1));
}
static void progress(int self,const std::string&phase){receipt(self,"progress","PHASE="+phase+"\n");}
static uint64_t free_kib(){struct statvfs info;ok(!statvfs("/data",&info),"tidy-free-space");require(info.f_frsize&&uint64_t(info.f_bavail)<=UINT64_MAX/uint64_t(info.f_frsize),"tidy-space-overflow");return uint64_t(info.f_bavail)*info.f_frsize/1024;}
static unsigned long system_flags(bool readonly){
    std::istringstream rows(read("/proc/self/mounts",1024*1024));std::string line;unsigned count=0;unsigned long flags=MS_REMOUNT;
    while(std::getline(rows,line)){std::istringstream row(line);std::string source,target,type,opts;require(bool(row>>source>>target>>type>>opts),"tidy-system-mount-row");if(target!="/system")continue;++count;std::istringstream options(opts);std::string option;while(std::getline(options,option,',')){if(option=="nosuid")flags|=MS_NOSUID;else if(option=="nodev")flags|=MS_NODEV;else if(option=="noexec")flags|=MS_NOEXEC;else if(option=="sync")flags|=MS_SYNCHRONOUS;else if(option=="dirsync")flags|=MS_DIRSYNC;else if(option=="noatime")flags|=MS_NOATIME;else if(option=="nodiratime")flags|=MS_NODIRATIME;else if(option=="relatime")flags|=MS_RELATIME;else if(option=="strictatime")flags|=MS_STRICTATIME;}}
    require(count==1,"tidy-system-mount-ambiguous");return flags|(readonly?MS_RDONLY:0);
}
static void remount(bool readonly){ok(!system_remount(system_flags(readonly)),"tidy-system-remount",90);struct statvfs st;ok(!statvfs("/system",&st),"tidy-system-mount-stat",90);require(bool(st.f_flag&ST_RDONLY)==readonly,"tidy-system-remount-confirm",90);}
static void generated(int self,unsigned&removed,unsigned&kept,int&restore_errno){
    (void)self;
    if(exists("/blackbox")){FD box(pathdir("/blackbox"));for(const char*name:{"x2d-shutter-audio.disable","x2d-shutter-audio.disable.next","x2d-shutter-audio.skip-once","x2d-liveview-timeout.txt"}){struct stat st;if(!state(box.n,name,st))continue;require(S_ISREG(st.st_mode)&&st.st_nlink==1,"tidy-blackbox-type",92);if(safe_remove(box.n,name,std::string("/blackbox/")+name,false))++removed;else ++kept;}}
    if(exists("/blackbox")){FD box(pathdir("/blackbox"));for(const auto&name:entries(box.n)){if(!liveview_temporary(name))continue;struct stat st;if(!state(box.n,name,st))continue;require(S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_uid==geteuid()&&st.st_size>=0&&st.st_size<=32,"tidy-liveview-temporary-type",92);if(safe_remove(box.n,name,"/blackbox/"+name,false))++removed;else ++kept;}}
    static const char*legacy[]={
        "cnl-1cb916ba","cnl-40040931","cnl-40040932","cnl-40040933","cnl-40040934","cnl-40040935",
        "cnl-40040940","cnl-40040942","cnl-40040950","cnl-40040952","cnl-40040955","cnl-40041012",
        "cnl-af000939","cnl-af000940","cnl-af000941","cnl-af000942","cnl-af000943","cnl-af000944","cnl-af000945","cnl-af000946","cnl-af000947",
        "cnl-d1500930","cnl-d1500931","cnl-d1500932","cnl-e5f10001","cnl-e5f10002","cnl-esflash-front-host",
        "cnl-fac100929","cnl-fac110929","cnl-fac120929","cnl-fac130929","cnl-fac140929","cnl-fac170929",
        "cnl-fac20929","cnl-fac30929","cnl-fac40929","cnl-fac50929","cnl-fac60929","cnl-fac70929","cnl-fac80929",
        "cnl-face0928","cnl-face0929","cnl-face0930","cnl-face0931","cnl-face0932","cnl-face0933","cnl-face0934","cnl-face0935","cnl-face0936","cnl-face0937","cnl-face0938",
        // 补齐旧版 native_disk400_cleanup.inc.sh 已登记而原生迁移遗漏的名称。
        "cnl-40040936","cnl-40040937","cnl-40040941","cnl-40040943","cnl-40040944","cnl-40040947","cnl-40040948","cnl-40040949","cnl-40040951","cnl-40040953","cnl-40040954","cnl-40040956","cnl-40040957","cnl-40040958","cnl-40040959","cnl-40040960","cnl-40041001","cnl-40041002","cnl-40041003","cnl-40041004","cnl-40041005","cnl-40041006","cnl-40041007","cnl-40041008","cnl-40041009","cnl-4004100a","cnl-4004100b","cnl-4004100c","cnl-4004100d","cnl-4004100e","cnl-4004100f","cnl-40041010","cnl-40041011","cnl-fac150929","cnl-fac160929","cnl-fac90929","x2d-four-3T2MIQ"};
    FD data(pathdir("/data"));for(const char*name:legacy){struct stat st;if(!state(data.n,name,st))continue;if(safe_remove(data.n,name,std::string("/data/")+name))++removed;else {
        if(std::string(name)=="x2d-four-3T2MIQ"){
            FD olddir(openat(data.n,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
            // 旧 FIFO 的属性可能不可读；只回收登记过的普通程序文件，绝不打开 FIFO 或照片。
            for(const char*file:{"io.sh","io64.sh","jpeg2","jpeg2.b64","jpeg2.log","loop","loop-io.sh","loop.b64","loop.log","loop2.log","onboard_four_64","onboard_four_64.b64","onboard_jpeg_arm","onboard_jpeg_arm.b64","run64-live.log","run64.log","stream","stream.b64","stream.log","timed","timed.b64","timed.log"}){
                if(state(olddir.n,file,st)&&safe_remove(olddir.n,file,std::string("/data/")+name+"/"+file,false))++removed;
            }
        }
        ++kept;
    }}
    // SSD 成片与拍摄任务资料属于用户输出，不作为补丁程序残留删除。
    bool mount_changed=false;
    try{
        // 多轮卸载工具更新可能留下不止一代 previous；仅回收完整摘要已登记的旧工具。
        {FD bin(pathdir("/system/bin"));for(const auto&name:entries(bin.n)){
            const std::string prefix="x2d-native-setup.previous.";const char*runtime_sha=nullptr;
            if(name=="x2d-native-flow.rcf1.old")runtime_sha="029025b0ea4c7984f163d0cc87720a63323ff73d9e35cdfd396e351a6606d91c";
            if(name=="x2d-native-flow.rcf2.old")runtime_sha="8d552fba867a3c578da427dc58222c0a4da64fc3609cb4c8c67a1d49e109a36f";
            if(!runtime_sha&&(name.rfind(prefix,0)!=0||!hex(name.substr(prefix.size()),16)))continue;
            struct stat st;require(state(bin.n,name,st)&&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_uid==0&&(st.st_mode&07777)==0755,"tidy-old-helper-type");
            const auto hash=sha256("/system/bin/"+name);bool known=runtime_sha&&hash==runtime_sha;if(!runtime_sha)for(const char*h:legacy_helper_hashes)if(hash==h)known=true;if(!known){++kept;continue;}
            if(!mount_changed){struct statvfs fs;ok(!statvfs("/system",&fs),"tidy-helper-system-stat");require(fs.f_flag&ST_RDONLY,"tidy-helper-system-not-readonly",92);mount_changed=true;remount(false);}
            if(safe_remove(bin.n,name,"/system/bin/"+name,false))++removed;else ++kept;
        }}
        // 精确历史私有库，摘要来自冻结构建与旧安装表，不按 *.old 扩大删除。
        static const LooseFile old_libraries[]={
            {"/system/bin/x2d-memory-manual-r17/libx2d_private_memory_capture.so.rmg.old","9c163ea49b3e5377865bd2f6c37ccef038c136f956f53e22abe4c5925da83765"},
            {"/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so.rmg2.old","93769d90ddb345768aa2a4f876a372b65c74849a65c4a13581e12bfcf5ac3cab"},
            {"/system/bin/x2d-raw-stack/libx2d_raw_stack.so.rcf1.old","fec4c14efdd7d61f3940ab881015bde199e61b21dd7b9ee6a963408626805091"},
            {"/system/bin/x2d-raw-stack/libx2d_raw_stack.so.rcf2.old","6e83ada29a70df257ebc3aadb85f3be897478435947a919b05445b3ab629db3e"},
            {"/system/bin/x2d-raw-stack/libx2d_raw_stack.so.rfc2.old","6fd25a12b8b5ea13ae3c38e763293e7cf82494e203ad2bbb209e7c6e0aa0c796"},
            {"/system/bin/x2d-raw-stack/libx2d_raw_stack.so.ru3.old","463f6057ca9fd08e9e5b1215720a6c85bcf2968f28b274da682c72526a1c2d37"},
        };
        for(const auto&file:old_libraries){
            const std::string path=file.name;auto split=path.rfind('/');if(!exists(path.substr(0,split)))continue;FD dir(pathdir(path.substr(0,split)));auto name=path.substr(split+1);struct stat st;
            if(!state(dir.n,name,st))continue;
            if(!S_ISREG(st.st_mode)||st.st_nlink!=1||st.st_uid!=0||sha256(path)!=file.sha){++kept;continue;}
            if(!mount_changed){struct statvfs fs;ok(!statvfs("/system",&fs),"tidy-library-system-stat");require(fs.f_flag&ST_RDONLY,"tidy-library-system-not-readonly",92);mount_changed=true;remount(false);}
            if(safe_remove(dir.n,name,path,false))++removed;else ++kept;
        }
        for(const char*parent:{"/system/bin","/system/etc"}){FD dir(pathdir(parent));std::vector<std::string> names={"x2d-pixel-overclock","x2d-raw-stack","x2d-multi-resolution"};if(std::string(parent)=="/system/bin")for(const char*n:{"x2d-native-disk400","x2d-memory-manual-r17","x2d-rs-check-r1","x2d-six-tile-check-r1"})names.emplace_back(n);
            for(const auto&name:names){struct stat st;if(!state(dir.n,name,st))continue;if(!S_ISDIR(st.st_mode)){++kept;continue;}bool empty;{FD child(openat(dir.n,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));empty=entries(child.n).empty();}if(!empty||references(true).used(std::string(parent)+"/"+name,st,true)){++kept;continue;}
                if(!mount_changed){struct statvfs fs;ok(!statvfs("/system",&fs),"tidy-initial-system-stat");require(fs.f_flag&ST_RDONLY,"tidy-initial-system-not-readonly",92);mount_changed=true;remount(false);}remove_owned(dir.n,name,st,AT_REMOVEDIR);ok(!fsync(dir.n),"tidy-system-directory-sync");++removed;
            }
        }
    }catch(const std::exception&e){if(mount_changed){Recovery cleanup;try{remount(true);}catch(const std::exception&restore){const auto*r=dynamic_cast<const Error*>(&restore);restore_errno=r?r->system_errno:0;fprintf(stderr,"TIDY_REMOUNT_RESTORE_UNCONFIRMED %s errno=%d\n",restore.what(),restore_errno);const auto*p=dynamic_cast<const Error*>(&e);throw Error(e.what(),p?p->rc:1,true,p?p->system_errno:0);}}throw;}
    if(mount_changed){Recovery cleanup;try{remount(true);}catch(const Error&e){restore_errno=e.system_errno;throw Error(e.what(),e.rc,true,e.system_errno);}}
}
static int run(const std::string&mode,const std::string&self,const std::string&attempt){
    require(mode=="before"||mode=="installed"||mode=="uninstalled","tidy-mode",2);const std::string prefix="/data/local/tmp/x2d-tidy-";require(self.rfind(prefix,0)==0&&hex(self.substr(prefix.size()),8)&&hex(attempt,16),"tidy-arguments",2);
    FD stage(pathdir(self)),base(pathdir("/data/local/tmp")),tmp(pathdir("/tmp"));struct stat st;require(!state(stage.n,"started",st)&&!state(stage.n,"result",st)&&!state(stage.n,"final",st),"tidy-stage-reused",3);receipt(stage.n,"started","NATIVE_DATA_CLEANUP_V1 "+attempt+"\n");
    Lock cleanup(tmp.n,"x2d-data-cleanup-lock"),capture(tmp.n,"x2d-multishot-active");int primary=0,recovery=0,primary_errno=0,restore_errno=0;std::string step="starting",first="none",outcome="complete";unsigned removed=0,kept=0,unknown=0,diagnostic=0;std::string candidate="none";uint64_t before=0,after=0;
    try{
        progress(stage.n,step);if(!cleanup.acquire())outcome="deferred-cleanup";else if(!capture.acquire())outcome="deferred-capture";
        if(outcome=="complete"){
            step="diagnostics";progress(stage.n,step);std::string latest;timespec stamp{};auto names=entries(base.n);
            // 诊断保留只适用于安装/清理事务；历史测试的 result 可以是大型测量结果。
            if(mode!="uninstalled")for(const auto&name:names){if(std::any_of(std::begin(legacy_stages),std::end(legacy_stages),[&](const LegacyStage&e){return name==e.name;}))continue;if(!stage_name(name)||name=="x2d-rs1"||name=="x2d-rs8"||self=="/data/local/tmp/"+name||!state(base.n,name,st)||!S_ISDIR(st.st_mode))continue;FD dir(openat(base.n,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));std::string marker;for(const char*f:{"exit","result"})if(ordinary(dir.n,f)){marker=f;break;}if(!marker.empty()){if(trim_newlines(content(dir.n,marker,256))=="0")continue;}else if(ordinary(dir.n,"started"))marker="started";else continue;require(state(dir.n,marker,st),"tidy-failure-marker-lost");if(st.st_mtim.tv_sec>stamp.tv_sec||(st.st_mtim.tv_sec==stamp.tv_sec&&st.st_mtim.tv_nsec>stamp.tv_nsec)){latest=name;stamp=st.st_mtim;}}
            before=free_kib();step="directories";progress(stage.n,step);
            for(const auto&name:names){cancelled();if(name.rfind("x2d-",0)!=0||self=="/data/local/tmp/"+name)continue;progress(stage.n,"directories removed="+std::to_string(removed)+" retained="+std::to_string(kept));if(name=="x2d-bootstrap"){++kept;++diagnostic;continue;}bool loose=false;for(const auto&file:legacy_loose)if(name==file.name){loose=true;candidate=name;if(ordinary(base.n,name)&&sha256("/data/local/tmp/"+name)==file.sha&&safe_remove(base.n,name,"/data/local/tmp/"+name,false))++removed;else ++kept;break;}if(loose)continue;if(!stage_name(name)){++kept;++unknown;continue;}if(name==latest&&mode!="uninstalled"){++kept;++diagnostic;continue;}candidate=name;if(!state(base.n,name,st)||!S_ISDIR(st.st_mode))continue;
                if(name=="x2d-rs1"&&mode!="uninstalled"){if(mode=="before"){++kept;continue;}FD dir(openat(base.n,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));if(!ordinary(dir.n,"result")||!ordinary(dir.n,"done")||trim_newlines(content(dir.n,"result",256))!="0"){++kept;continue;}auto done=trim_newlines(content(dir.n,"done",256));if(done!="RAW_STACK_BACKEND_INSTALLED_NO_CAPTURE"&&done!="RAW_STACK_STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED"){++kept;continue;}}
                if(name.rfind("x2d-un-",0)==0&&exists("/system/x2d-un-retained-"+name.substr(7))){++kept;continue;}
                if(legacy_owned(base.n,name)&&safe_remove(base.n,name,"/data/local/tmp/"+name))++removed;else ++kept;
            }
            if(mode=="uninstalled"){step="project-generated";progress(stage.n,step);generated(stage.n,removed,kept,restore_errno);}after=free_kib();if(kept>diagnostic)outcome="deferred-retained";progress(stage.n,outcome);
        }
    }catch(const Error&e){primary=e.rc;primary_errno=e.system_errno;first=step+":"+e.what();if(e.recovery_failed)recovery=90;}catch(const std::exception&e){primary=1;first=step+":"+e.what();}
    {Recovery finish;if(!recovery){try{capture.release();}catch(const std::exception&e){recovery=90;const auto*p=dynamic_cast<const Error*>(&e);restore_errno=p?p->system_errno:0;fprintf(stderr,"TIDY_UNLOCK_UNCONFIRMED %s errno=%d\n",e.what(),restore_errno);}if(!recovery)try{cleanup.release();}catch(const std::exception&e){recovery=90;const auto*p=dynamic_cast<const Error*>(&e);restore_errno=p?p->system_errno:0;fprintf(stderr,"TIDY_UNLOCK_UNCONFIRMED %s errno=%d\n",e.what(),restore_errno);}}if(recovery)fprintf(stderr,"TIDY_RECOVERY_UNCONFIRMED_LOCKS_RETAINED\n");if(!primary&&cn_signal_pending()){primary=143;first="cancelled";}
        try{receipt(stage.n,"result",std::to_string(recovery?recovery:primary)+"\n");receipt(stage.n,"final","NATIVE_TRANSACTION_V1\noperation=data-cleanup\ntransaction="+self.substr(prefix.size())+"\nattempt="+attempt+"\nmode="+mode+"\noutcome="+outcome+"\nprimary_step="+first+"\nprimary_errno="+(primary_errno?std::to_string(primary_errno):"UNAVAILABLE")+"\nrestore_errno="+(restore_errno?std::to_string(restore_errno):"UNAVAILABLE")+"\nprimary_rc="+std::to_string(primary)+"\nrestore_rc="+std::to_string(recovery)+"\nfinal_rc="+std::to_string(recovery?recovery:primary)+"\nremoved="+std::to_string(removed)+"\nretained="+std::to_string(kept)+"\nretained_unknown="+std::to_string(unknown)+"\nretained_diagnostic="+std::to_string(diagnostic)+"\nactive_candidate="+candidate+"\nfree_before_kib="+std::to_string(before)+"\nfree_after_kib="+std::to_string(after)+"\n");}catch(const std::exception&e){fprintf(stderr,"TIDY_RECEIPT_UNCONFIRMED %s\n",e.what());return 90;}}
    return recovery?recovery:primary;
}
} }
extern "C" int cn_data_cleanup_main(int argc,char**argv){try{cn::require(argc==5&&std::string(argv[1])=="data-cleanup","tidy-arguments",2);cn::TransactionGuard transaction;return cn::tidy::run(argv[2],argv[3],argv[4]);}catch(const cn::Error&e){fprintf(stderr,"NATIVE_DATA_CLEANUP_REFUSED step=%s rc=%d errno=%d\n",e.what(),e.rc,e.system_errno);return e.recovery_failed?90:e.rc;}catch(const std::exception&e){fprintf(stderr,"NATIVE_DATA_CLEANUP_REFUSED detail=%s\n",e.what());return 1;}}


