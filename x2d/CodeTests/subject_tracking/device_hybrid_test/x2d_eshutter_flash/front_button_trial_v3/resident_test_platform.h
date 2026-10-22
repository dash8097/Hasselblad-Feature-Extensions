#include <stdint.h>
#include <stddef.h>
#define clock_gettime test_clock_gettime
#define waitpid test_waitpid
#define getpid test_getpid
#define getppid test_getppid
#define fork test_fork
#define poll test_poll
#define dlopen test_dlopen
#define dlclose test_dlclose
#define dlsym test_dlsym
#define mmap test_mmap
#define munmap test_munmap
#define _exit test_exit
extern int waitpid(int,int*,int),getpid(void),getppid(void),fork(void),munmap(void*,unsigned long),dlclose(void*);
extern void *dlopen(const char*,int),*dlsym(void*,const char*),*mmap(void*,unsigned long,int,int,int,long);
extern void _exit(int) __attribute__((noreturn));
static int selected_process(void){return 1;}
static int verified_binary(void){return 0;}
typedef struct { uintptr_t mapped,physical,length; } NativeMap;
typedef int (*init_fn)(void);
typedef void (*deinit_fn)(void);
typedef volatile uint32_t *(*addr_fn)(unsigned,uintptr_t);
typedef void (*set_fn)(int);
typedef void (*sleep_fn)(unsigned);
typedef unsigned (*product_fn)(void);
static int mapped_ok(const NativeMap*m){return m!=0;}
