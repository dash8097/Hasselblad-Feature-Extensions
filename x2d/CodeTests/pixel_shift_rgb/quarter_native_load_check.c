/* 仅核对原厂动态链接器能加载候选模块；不映射图像池、不拍摄。 */
#include <dlfcn.h>
#include <stdio.h>
static int check(int argc,char **argv,char **env){
 (void)argc;(void)argv;(void)env;
 void *m=dlopen("/system/lib64/libx2d_quarter_native.so",RTLD_NOW|RTLD_LOCAL);
 if(!m){const char *e=dlerror();fprintf(stderr,"QUARTER_NATIVE_LINK_FAILED %s\n",e?e:"unknown");return 1;}
 int(*check)(unsigned*,unsigned*,unsigned*)=(int(*)(unsigned*,unsigned*,unsigned*))dlsym(m,"qn_storage_metrics");
 unsigned a=1,b=1,c=1;
 if(!check||check(&a,&b,&c)!=1||a||b||c){dlclose(m);return 2;}
 dlclose(m);puts("QUARTER_NATIVE_LINK_VERIFIED_NO_IMAGE_POOL_NO_CAPTURE");return 0;
}
typedef struct {void (**preinit)(void),(**init)(void),(**fini)(void);} Structors;
extern __attribute__((noreturn))void __libc_init(void *,void (*)(void),int (*)(int,char**,char**),const Structors *);
__attribute__((noreturn,used))void stack_entry(void *stack){const Structors empty={0};__libc_init(stack,0,check,&empty);}
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0, sp\nbl stack_entry");}
