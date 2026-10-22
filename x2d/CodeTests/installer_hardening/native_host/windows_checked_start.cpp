/* 不链接 CPython；先验包，随后固定启动已校验的 engine。 */
#include <windows.h>
#include <string>
#include "package_guard.hpp"
int wmain(int argc,wchar_t**argv){
    wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);
    const bool verify_only=argc==3 && std::wstring(argv[1])==L"--verify-package";
    if((argc!=2 && !verify_only) || !n || n>=32768)return 2;
    auto root=std::filesystem::path(path).parent_path();
    package_guard::Lease verified_package;
    if(std::filesystem::path(argv[verify_only?2:1]).lexically_normal()!=root.lexically_normal() ||
       !package_guard::verify(root,"X2dStart.exe",verified_package)){
        fputs("PACKAGE_NATIVE_PREFLIGHT_FAILED\n",stderr);return 2;
    }
    if(verify_only){fputs("PACKAGE_NATIVE_PREFLIGHT_OK\n",stdout);return 0;}
    const auto engine=(root/L"X2dEngine.exe").wstring();
    std::wstring command=L"\""+engine+L"\" \""+root.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);startup.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);startup.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION child{};
    if(!CreateProcessW(engine.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&child))return 2;
    CloseHandle(child.hThread);DWORD waited=WaitForSingleObject(child.hProcess,INFINITE),code=2;
    if(waited==WAIT_OBJECT_0)GetExitCodeProcess(child.hProcess,&code);
    CloseHandle(child.hProcess);return int(code);
}
