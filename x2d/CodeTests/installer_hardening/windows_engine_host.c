#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <windows.h>
#include <stdio.h>

/* 仅固定 bootstrap；安装业务编译在 pyd 中，不解包执行脚本。 */
int wmain(int argc,wchar_t **argv) {
    if(argc!=2) return 2;
    wchar_t exe[32768],directory[32768];
    DWORD n=GetModuleFileNameW(NULL,exe,32768);
    if(!n || n>=32768) return 2;
    wchar_t *slash=wcsrchr(exe,L'\\');if(!slash)return 2;*slash=0;
    if(swprintf(directory,32768,L"%ls\\runtime",exe)<0)return 2;
    PyConfig config;PyConfig_InitIsolatedConfig(&config);
    config.write_bytecode=0;config.site_import=0;config.parse_argv=0;
    config.install_signal_handlers=0;config.use_environment=0;config.user_site_directory=0;
    PyStatus status=PyConfig_SetString(&config,&config.home,directory);
    if(PyStatus_Exception(status))goto error;
    status=PyConfig_SetString(&config,&config.stdio_encoding,L"utf-8");
    if(PyStatus_Exception(status))goto error;
    status=PyConfig_SetArgv(&config,argc,argv);
    if(PyStatus_Exception(status))goto error;
    config.module_search_paths_set=1;
    wchar_t path[32768];
    const wchar_t *suffixes[]={L"\\runtime\\python311.zip",L"\\runtime\\DLLs",L"\\engine"};
    for(unsigned i=0;i<3;i++) {
        if(swprintf(path,32768,L"%ls%ls",exe,suffixes[i])<0)goto error;
        status=PyWideStringList_Append(&config.module_search_paths,path);
        if(PyStatus_Exception(status))goto error;
    }
    status=Py_InitializeFromConfig(&config);
    if(PyStatus_Exception(status))goto error;
    PyConfig_Clear(&config);
    int rc=PyRun_SimpleString("import windows_engine; windows_engine.main()");
    if(Py_FinalizeEx()<0)return 120;
    return rc?1:0;
error:
    PyConfig_Clear(&config);fputs("NATIVE_ENGINE_INITIALIZATION_FAILED\n",stderr);return 2;
}
