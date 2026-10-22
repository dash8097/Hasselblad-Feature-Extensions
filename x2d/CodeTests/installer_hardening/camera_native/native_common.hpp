#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/types.h>
namespace cn {
struct Error : std::runtime_error {
    int rc;
    bool recovery_failed;
    int system_errno;
    Error(const std::string &step,int code=1,bool recovery=false,int cause=0):std::runtime_error(step),rc(code),recovery_failed(recovery),system_errno(cause){}
};
void require(bool,const std::string&,int=1);
void require(bool,const char*,int=1);
struct Recovery { Recovery(); ~Recovery(); Recovery(const Recovery&)=delete; Recovery& operator=(const Recovery&)=delete; };
// 独立于拍摄任务锁；在卸载解除准备状态之前序列化所有安装事务。
struct TransactionGuard {
    int fd=-1;
    TransactionGuard(); ~TransactionGuard();
    TransactionGuard(const TransactionGuard&)=delete;
    TransactionGuard& operator=(const TransactionGuard&)=delete;
};
void cancelled();
void cancel_before_capture(const std::string&job);
bool stopping();
void sleep_ms(unsigned);
uint64_t monotonic_ms();
bool exists(const std::string&);
bool regular(const std::string&,bool=false);
bool directory(const std::string&);
void job(const std::string&);
std::string read(const std::string&,size_t=65536);
std::string trim_newlines(std::string);
std::string value(const std::string&,size_t=65536);
void write_new(const std::string&,const std::string&);
void write_replace(const std::string&,const std::string&);
void remove_regular(const std::string&);
void append(const std::string&,const std::string&);
void mkdir_new(const std::string&);
void move_new(const std::string&,const std::string&,bool=true);
void move_publish_new(const std::string&,const std::string&);
void copy_new(const std::string&,const std::string&);
bool same_device(const std::string&,const std::string&);
std::string sha256(const std::string&);
bool mapped_library(const std::vector<std::string>&);
struct Process { int rc; std::string output; };
Process command(const std::vector<std::string>&,unsigned=6000,bool=true);
int system_remount(unsigned long flags);
std::string get(const std::string&,const std::string&,const std::string&,unsigned=3000);
std::string getprop(const std::string&);
void setprop(const std::string&,const std::string&);
void service(const std::string&,bool);
std::vector<pid_t> pids(const std::string&);
std::string executable(pid_t);
bool digits(const std::string&);
struct Album { std::string folder,image; unsigned folder_number,file_number; };
Album album_plan(const std::string&);
int counter_main(int,char**);
int publish_main(int,char**);
}
