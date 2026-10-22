/* 固定 Mac 原生入口；在导入任何业务模块前核验 Resources 全部文件。 */
#include <unistd.h>
#include <stdint.h>
#include <cstdlib>
#include <dlfcn.h>
#include "package_guard.hpp"
extern "C" int _NSGetExecutablePath(char*,uint32_t*);
// Finder 启动没有可见终端；原生错误提示也不加载包内业务模块。
static int startup_error(){
    fputs("PACKAGE_NATIVE_PREFLIGHT_FAILED\n",stderr);
    void* framework=dlopen("/System/Library/Frameworks/AppKit.framework/AppKit",RTLD_LAZY);
    void* runtime=dlopen("/usr/lib/libobjc.A.dylib",RTLD_LAZY);
    if(framework && runtime){
        auto cls=reinterpret_cast<void*(*)(const char*)>(dlsym(runtime,"objc_getClass"));
        auto sel=reinterpret_cast<void*(*)(const char*)>(dlsym(runtime,"sel_registerName"));
        void* msg=dlsym(runtime,"objc_msgSend");
        if(cls && sel && msg){
            auto send=reinterpret_cast<void*(*)(void*,void*)>(msg);
            auto argument=reinterpret_cast<void*(*)(void*,void*,void*)>(msg);
            auto string=reinterpret_cast<void*(*)(void*,void*,const char*)>(msg);
            void* pool=send(cls("NSAutoreleasePool"),sel("new"));
            void* alert=send(cls("NSAlert"),sel("new"));
            argument(alert,sel("setMessageText:"),string(cls("NSString"),sel("stringWithUTF8String:"),"安装客户端无法启动"));
            argument(alert,sel("setInformativeText:"),string(cls("NSString"),sel("stringWithUTF8String:"),"安装包不完整或校验未通过。请重新完整解压安装包。尚未连接或操作相机。"));
            send(alert,sel("runModal"));send(pool,sel("drain"));
        }
    }
    return 2;
}
int main(){
    char path[4096],resolved[4096];uint32_t size=sizeof path;
    if(_NSGetExecutablePath(path,&size) || !realpath(path,resolved))return startup_error();
    auto contents=std::filesystem::path(resolved).parent_path().parent_path();
    // 清单根为 Contents；启动器自身不进入自己的摘要清单。
    if(!package_guard::verify(contents,"MacOS/X2DInstaller")){
        return startup_error();
    }
    auto resources=contents/"Resources";
#if defined(__aarch64__)
    auto runtime=resources/"runtime/arm64/bin/python3.12";
#elif defined(__x86_64__)
    auto runtime=resources/"runtime/x86_64/bin/python3.12";
#else
#error unsupported architecture
#endif
    std::string executable=runtime.string(),root=resources.string();
    char isolated[]="-I",no_cache[]="-B",command_flag[]="-c";
    char command[]="import sys;sys.path.insert(0,sys.argv[1]);import mac_gui;mac_gui.main()";
    char*args[]={executable.data(),isolated,no_cache,command_flag,command,root.data(),nullptr};
    execv(executable.c_str(),args);return startup_error();
}
