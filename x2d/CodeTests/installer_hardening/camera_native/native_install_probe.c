/* 固定 4.2.0 Bionic 动态加载探针：只检查本进程，不连接相机服务。 */
#define _GNU_SOURCE
#include <link.h>
#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
extern int bionic_sscanf(const char *,const char *,...) __asm__("sscanf");
static unsigned linux_major(uint64_t d){return (unsigned)(((d>>8)&0xfff)|((d>>32)&0xfffff000));}
static unsigned linux_minor(uint64_t d){return (unsigned)((d&0xff)|((d>>12)&0xffffff00));}

static const char library[]="/system/bin/x2d-rs-check-r1/library.so";
struct Observation { unsigned count, executable, dynamic; uintptr_t base; };
static int observe(struct dl_phdr_info *info,size_t length,void *context){
    struct Observation *out=context;
    if(length<offsetof(struct dl_phdr_info,dlpi_phnum)+sizeof info->dlpi_phnum ||
       !info->dlpi_name || strcmp(info->dlpi_name,library))return 0;
    ++out->count;
    if(!info->dlpi_addr || !info->dlpi_phdr || !info->dlpi_phnum || info->dlpi_phnum>128)return 1;
    const Elf64_Ehdr *header=(const void *)(uintptr_t)info->dlpi_addr;
    if(memcmp(header->e_ident,ELFMAG,SELFMAG)||header->e_ident[EI_CLASS]!=ELFCLASS64||
       header->e_ident[EI_DATA]!=ELFDATA2LSB||header->e_machine!=EM_AARCH64||
       header->e_type!=ET_DYN||header->e_phnum!=info->dlpi_phnum)return 1;
    out->base=info->dlpi_addr;
    for(unsigned i=0;i<info->dlpi_phnum;++i){
        const Elf64_Phdr *p=&info->dlpi_phdr[i];
        if(p->p_type==PT_LOAD&&(p->p_flags&PF_X)&&p->p_memsz)out->executable=1;
        if(p->p_type==PT_DYNAMIC&&p->p_memsz)out->dynamic=1;
    }
    return 0;
}
static int probe_main(int argc,char **argv,char **envp){
    (void)argv;(void)envp;
    if(argc!=1)return 2;
    const char *abi=getenv("X2D_RAW_STACK_ABI"),*preload=getenv("LD_PRELOAD");
    if((abi&&*abi)||!preload||strcmp(preload,library))return 3;
    struct Observation out={0};
    if(dl_iterate_phdr(observe,&out)||out.count!=1||!out.base||!out.executable||!out.dynamic)return 4;
    int fd=open(library,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    struct stat st;if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1){if(fd>=0)close(fd);return 5;}
    if(close(fd))return 5;
    fd=open("/proc/self/maps",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return 6;
    FILE *maps=fdopen(fd,"r");if(!maps){close(fd);return 6;}
    char line[1024],permissions[5],path[512];unsigned found=0,lines=0;
    while(fgets(line,sizeof line,maps)){
        if(++lines>32768||!strchr(line,'\n')){fclose(maps);return 7;}
        unsigned long lo,hi,offset,inode;unsigned maj,min;
        int fields=bionic_sscanf(line,"%lx-%lx %4s %lx %x:%x %lu %511[^\n]",&lo,&hi,permissions,&offset,&maj,&min,&inode,path);
        if(fields<7){fclose(maps);return 7;}
        if(fields==8&&!strcmp(path,library)&&inode==(unsigned long)st.st_ino&&
           maj==linux_major(st.st_dev)&&min==linux_minor(st.st_dev)&&lo<hi&&permissions[2]=='x')found=1;
    }
    int failed=ferror(maps);if(fclose(maps)||failed||!found)return 8;
    static const char marker[]="NATIVE_INSTALL_PRELOAD_MAPPED_HOOKS_OFF\n";
    return write(1,marker,sizeof marker-1)==sizeof marker-1?0:9;
}
typedef struct {void (**preinit)(void),(**init)(void),(**fini)(void);} Structors;
extern __attribute__((noreturn))void __libc_init(void *,void (*)(void),int (*)(int,char**,char**),const Structors *);
__attribute__((noreturn,used))void install_probe_entry(void *stack){const Structors arrays={0};__libc_init(stack,NULL,probe_main,&arrays);}
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0, sp\nbl install_probe_entry");}
