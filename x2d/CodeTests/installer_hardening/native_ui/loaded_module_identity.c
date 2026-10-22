#define _GNU_SOURCE
#include "loaded_module_identity.h"
#include "receipt_sha256.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
/* Compile-time GNU headers redirect sscanf to a glibc-only symbol. The camera
 * uses the verified Bionic sscanf ABI, also provided by the Linux test libc. */
extern int x2d_bionic_sscanf(const char*,const char*,...) __asm__("sscanf");
#define MODULE_PATH "/system/lib64/libx2d_native_menu.so"
static int file_ok(const struct stat*s){return S_ISREG(s->st_mode)&&s->st_nlink==1&&(s->st_uid==0||s->st_uid==geteuid())&&!(s->st_mode&0022)&&s->st_size>=64&&s->st_size<=32*1024*1024;}
static int unchanged(const struct stat*a,const struct stat*b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;}
static int mapped(const struct stat*s){
 uintptr_t address=(uintptr_t)&x2d_loaded_module_hash;
#ifdef X2D_IDENTITY_TEST
 extern uintptr_t x2d_identity_test_address;address=x2d_identity_test_address;
#endif
 FILE*f=fopen("/proc/self/maps","re");if(!f)return 0;char line[1024];unsigned lines=0,hits=0;int ok=1;
 while(fgets(line,sizeof line,f)){
  if(++lines>8192||!strchr(line,'\n')){ok=0;break;}
  unsigned long long begin=0,end=0,offset=0,inode=0;unsigned major=0,minor=0;char flags[5]={0};int n=0;
  if(x2d_bionic_sscanf(line,"%llx-%llx %4s %llx %x:%x %llu %n",&begin,&end,flags,&offset,&major,&minor,&inode,&n)!=7||!n||begin>=end){ok=0;break;}
  if(address<begin||address>=end)continue;
  uint64_t dev=(uint64_t)s->st_dev;unsigned expected_major=(unsigned)((dev>>8)&0xfff)|((unsigned)(dev>>32)&0xfffff000);unsigned expected_minor=(unsigned)(dev&0xff)|((unsigned)(dev>>12)&0xffffff00);
  char*path=line+n;size_t length=strlen(path);if(length&&path[length-1]=='\n')path[--length]=0;
  if(strcmp(flags,"r-xp")||strcmp(path,MODULE_PATH)||inode!=(unsigned long long)s->st_ino||major!=expected_major||minor!=expected_minor||offset>=(uint64_t)s->st_size){ok=0;break;}++hits;
 }
 if(ferror(f))ok=0;if(fclose(f))ok=0;return ok&&hits==1;
}
int x2d_loaded_module_hash(char out[65]){
 if(!out)return 0;out[0]=0;int system=-1,lib=-1,fd=-1,ok=0;struct stat before,after,path;unsigned char buffer[32768],digest[32];off_t bytes=0;ReceiptSha hash;rsha_init(&hash);
 system=open("/system",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(system<0)goto done;
 lib=openat(system,"lib64",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(lib<0)goto done;
 fd=openat(lib,"libx2d_native_menu.so",O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);if(fd<0||fstat(fd,&before)||!file_ok(&before)||!mapped(&before))goto done;
 for(unsigned attempts=0;attempts<4096;attempts++){ssize_t n=read(fd,buffer,sizeof buffer);if(n<0){if(errno==EINTR)continue;goto done;}if(!n)break;bytes+=n;if(bytes>before.st_size)goto done;rsha_update(&hash,buffer,(unsigned)n);}
 if(bytes!=before.st_size||fstat(fd,&after)||!file_ok(&after)||!unchanged(&before,&after)||fstatat(lib,"libx2d_native_menu.so",&path,AT_SYMLINK_NOFOLLOW)||!file_ok(&path)||!unchanged(&before,&path)||!mapped(&before))goto done;
 rsha_final(&hash,digest);for(unsigned i=0;i<32;i++){out[2*i]="0123456789abcdef"[digest[i]>>4];out[2*i+1]="0123456789abcdef"[digest[i]&15];}out[64]=0;ok=1;
done:if(fd>=0&&close(fd))ok=0;if(lib>=0)close(lib);if(system>=0)close(system);if(!ok)out[0]=0;return ok;
}
