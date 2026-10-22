/* 原生启动前核验固定完整文件清单；不解析可改写的外部信任锚。 */
#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <cstdio>
#include <cstring>
#include <vector>
#include "portable_sha256.h"
#include "package_entries.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
namespace package_guard {
namespace fs=std::filesystem;
#ifdef _WIN32
// 生命周期属于等待业务子进程的启动器；句柄不继承，不授予写/删除共享。
struct Lease {
    std::vector<HANDLE> handles;
    std::set<fs::path> directories;
    Lease()=default;
    Lease(const Lease&)=delete;Lease&operator=(const Lease&)=delete;
    Lease(Lease&&other)noexcept{handles.swap(other.handles);directories.swap(other.directories);}
    Lease&operator=(Lease&&other)noexcept{if(this!=&other){clear();handles.swap(other.handles);directories.swap(other.directories);}return *this;}
    ~Lease(){clear();}
    void clear()noexcept{for(auto handle:handles)CloseHandle(handle);handles.clear();directories.clear();}
    void keep(HANDLE handle){try{handles.push_back(handle);}catch(...){CloseHandle(handle);throw;}}
};
static bool pin_directory(const fs::path&path,Lease&lease,bool package_directory){
    if(lease.directories.count(path))return true;
    const DWORD sharing=package_directory?FILE_SHARE_READ:(FILE_SHARE_READ|FILE_SHARE_WRITE);
    HANDLE h=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES|FILE_LIST_DIRECTORY,sharing,nullptr,OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(h,&info)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||
       (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)){CloseHandle(h);return false;}
    lease.keep(h);lease.directories.insert(path);return true;
}
static bool pin_root(const fs::path&root,Lease&lease){
    if(!root.is_absolute()||root!=root.lexically_normal())return false;
    auto current=root.root_path();
    if(!pin_directory(current,lease,current==root))return false;
    for(const auto&part:root.relative_path()){
        if(part.empty()||part==L"."||part==L"..")return false;
        current/=part;if(!pin_directory(current,lease,current==root))return false;
    }
    return current==root;
}
static bool pin_file(const fs::path&path,Lease&lease,const char*expected=nullptr,unsigned long long size=0){
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                         FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION before{},after{};
    auto ordinary_handle=[](const BY_HANDLE_FILE_INFORMATION&i){return !(i.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&i.nNumberOfLinks==1;};
    if(!GetFileInformationByHandle(h,&before)||!ordinary_handle(before)){CloseHandle(h);return false;}
    if(expected){
        if((static_cast<unsigned long long>(before.nFileSizeHigh)<<32|before.nFileSizeLow)!=size){CloseHandle(h);return false;}
        ReceiptSha hash;rsha_init(&hash);unsigned char data[65536],raw[32];unsigned long long count=0;
        for(;;){DWORD got=0;if(!ReadFile(h,data,sizeof data,&got,nullptr)){CloseHandle(h);return false;}if(!got)break;count+=got;if(count>size){CloseHandle(h);return false;}rsha_update(&hash,data,got);}
        if(count!=size){CloseHandle(h);return false;}rsha_final(&hash,raw);char text[65];
        for(unsigned i=0;i<32;++i){text[2*i]="0123456789abcdef"[raw[i]>>4];text[2*i+1]="0123456789abcdef"[raw[i]&15];}text[64]=0;
        if(strcmp(text,expected)){CloseHandle(h);return false;}
    }
    if(!GetFileInformationByHandle(h,&after)||!ordinary_handle(after)||
       before.dwVolumeSerialNumber!=after.dwVolumeSerialNumber||before.nFileIndexHigh!=after.nFileIndexHigh||before.nFileIndexLow!=after.nFileIndexLow||
       before.nFileSizeHigh!=after.nFileSizeHigh||before.nFileSizeLow!=after.nFileSizeLow||
       CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)!=0){CloseHandle(h);return false;}
    lease.keep(h);return true;
}
static bool verify(const fs::path&root,const char*self,Lease&retained){
    try{
        Lease pending;if(!pin_root(root,pending))return false;
        std::set<std::string> expected;expected.insert(self);
        if(!pin_file(root/fs::u8path(self),pending))return false;
        for(const auto&entry:package_entries){
            fs::path relative=fs::u8path(entry.name);
            if(relative.is_absolute()||relative.has_root_name()||relative.empty()||!expected.insert(entry.name).second)return false;
            fs::path current=root;size_t index=0,total=size_t(std::distance(relative.begin(),relative.end()));
            for(const auto&part:relative){auto text=part.native();if(text.empty()||part==L"."||part==L".."||text.find(L':')!=std::wstring::npos||text.back()==L'.'||text.back()==L' ')return false;
                current/=part;if(++index<total&&!pin_directory(current,pending,true))return false;
            }
            // Windows 用户策略：只检查必需文件存在、可用；不做全包摘要核验。
            if(!pin_file(current,pending))return false;
        }
        // 额外文件和无关 TXT 均不影响安装。
        retained=std::move(pending);return true;
    }catch(...){return false;}
}
#endif
#ifndef _WIN32
static bool ordinary(const fs::path&path,bool directory){
    auto s=fs::symlink_status(path);
    if(fs::is_symlink(s))return false;
#ifdef _WIN32
    DWORD a=GetFileAttributesW(path.c_str());
    if(a==INVALID_FILE_ATTRIBUTES || (a&FILE_ATTRIBUTE_REPARSE_POINT))return false;
#endif
    return directory?fs::is_directory(s):fs::is_regular_file(s);
}
static bool file_hash(const fs::path&path,const char*expected,unsigned long long size){
    if(!ordinary(path,false) || fs::file_size(path)!=size)return false;
#ifdef _WIN32
    FILE* stream=_wfopen(path.c_str(),L"rb");
#else
    FILE* stream=fopen(path.c_str(),"rb");
#endif
    if(!stream)return false;
    ReceiptSha hash;rsha_init(&hash);unsigned char data[65536],raw[32];
    unsigned long long count=0;size_t n;
    while((n=fread(data,1,sizeof data,stream))){count+=n;if(count>size){fclose(stream);return false;}rsha_update(&hash,data,unsigned(n));}
    bool failed=ferror(stream)!=0;fclose(stream);
    if(failed || count!=size || !ordinary(path,false) || fs::file_size(path)!=size)return false;
    rsha_final(&hash,raw);char text[65];
    for(unsigned i=0;i<32;++i){text[i*2]="0123456789abcdef"[raw[i]>>4];text[i*2+1]="0123456789abcdef"[raw[i]&15];}text[64]=0;
    return !strcmp(text,expected);
}
static bool verify(const fs::path&root,const char*self){
    try{
        if(!ordinary(root,true))return false;
        std::set<std::string> expected;
        expected.insert(self);
        for(const auto&entry:package_entries){
            fs::path relative=fs::u8path(entry.name);
            if(relative.is_absolute() || relative.empty() || !expected.insert(entry.name).second)return false;
            fs::path current=root;size_t parts=0,total=size_t(std::distance(relative.begin(),relative.end()));
            for(const auto&part:relative){
                if(part=="." || part=="..")return false;
                current/=part;if(++parts<total && !ordinary(current,true))return false;
            }
            if(!file_hash(current,entry.sha256,entry.bytes))return false;
        }
        std::set<std::string> actual;
        for(const auto&entry:fs::recursive_directory_iterator(root)){
            if(ordinary(entry.path(),true))continue;
            if(!ordinary(entry.path(),false))return false;
            actual.insert(entry.path().lexically_relative(root).generic_u8string());
        }
        return actual==expected;
    }catch(...){return false;}
}
#endif
}
