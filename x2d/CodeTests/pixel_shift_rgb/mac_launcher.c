/* macOS 原生小启动器；不打开 USB、不启动外部控制程序。
 * 仅启动本 .app 内固定架构的 Python / AppKit 界面，路径支持中文及空格。
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
extern int _NSGetExecutablePath(char *, uint32_t *);

static void startup_error(void) {
    void *appkit = dlopen("/System/Library/Frameworks/AppKit.framework/AppKit", RTLD_LAZY);
    void *objc = dlopen("/usr/lib/libobjc.A.dylib", RTLD_LAZY);
    if (appkit && objc) {
        void *(*cls)(const char *) = (void *(*)(const char *))dlsym(objc,"objc_getClass");
        void *(*sel)(const char *) = (void *(*)(const char *))dlsym(objc,"sel_registerName");
        void *msg = dlsym(objc,"objc_msgSend");
        if (cls && sel && msg) {
            void *(*send)(void *, void *) = (void *(*)(void *, void *))msg;
            void *(*send_arg)(void *, void *, void *) = (void *(*)(void *, void *, void *))msg;
            void *pool = send(cls("NSAutoreleasePool"),sel("new"));
            void *alert = send(cls("NSAlert"),sel("new"));
            void *text = send_arg(cls("NSString"),sel("stringWithUTF8String:"),
                "安装客户端无法启动。请完整解压安装包后重试；未连接或操作相机。");
            send_arg(alert,sel("setMessageText:"),text);
            send(alert,sel("runModal"));
            send(pool,sel("drain"));
            return;
        }
    }
    fputs("X2D installer cannot start; extract the complete package.\n",stderr);
}

int main(void) {
    char executable[4096], resolved[4096], runtime[4096], gui[4096];
    uint32_t length = sizeof(executable);
    if (_NSGetExecutablePath(executable,&length) || !realpath(executable,resolved)) {
        startup_error(); return 1;
    }
    char *last = strrchr(resolved,'/');
    if (!last) { startup_error(); return 1; }
    *last = 0;
#if defined(__aarch64__)
    const char *architecture = "arm64";
#elif defined(__x86_64__)
    const char *architecture = "x86_64";
#else
#error unsupported architecture
#endif
    int a = snprintf(runtime,sizeof(runtime),"%s/../Resources/runtime/%s/bin/python3.12",resolved,architecture);
    int b = snprintf(gui,sizeof(gui),"%s/../Resources/mac_gui.py",resolved);
    if (a <= 0 || (size_t)a >= sizeof(runtime) || b <= 0 || (size_t)b >= sizeof(gui) ||
        access(runtime,X_OK) || access(gui,R_OK)) { startup_error(); return 1; }
    char *args[] = {runtime,"-I","-B",gui,NULL};
    execv(runtime,args);
    startup_error(); return 1;
}
