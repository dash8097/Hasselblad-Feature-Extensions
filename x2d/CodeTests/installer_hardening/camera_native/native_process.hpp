#pragma once
#include "native_common.hpp"
namespace cn {
// 仅管理本进程 fork 的子进程。wait 后不再向该 PID 发信号。
// 析构只等待；显影可能持有原厂 RAW/Ienc 引用，不能任意 TERM/free。
class Child {
    pid_t pid_=-1;
    bool done_=false;
    int rc_=1;
    bool drain_on_destroy_=true;
    bool cancellable_utility_=false;
    bool independent_heif_=false;
public:
    explicit Child(bool drain_on_destroy=true):drain_on_destroy_(drain_on_destroy){}
    ~Child();
    Child(const Child&)=delete;
    Child& operator=(const Child&)=delete;
    void start(const std::vector<std::string>&,const std::string&);
    pid_t pid()const{return pid_;}
    bool done()const{return done_;}
    bool poll();
    int wait(bool interruptible=false);
    int wait_utility();
    void terminate_after_release(const std::string&);
    void terminate_heif_bounded();
};
unsigned log_count(const std::string&,const std::string&);
unsigned saved_count(const std::string&,const std::string&);
bool log_prefix(const std::string&,const std::string&);
void lease_remove(const std::string&,pid_t);
}
