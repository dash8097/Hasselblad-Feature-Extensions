#ifdef X2D_UI_PRODUCTION
#define X2D_UI_INTERNAL_API __attribute__((visibility("hidden")))
#else
#define X2D_UI_INTERNAL_API __attribute__((visibility("default")))
#endif
/* X2D 4.2.0 read-only process ABI probe. No constructor/hook/device operation. */
#define _GNU_SOURCE
#include <link.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "bridge_core.h"
#include "gui_profile.h"
#include "receipt_sha256.h"
#include "slot_core.h"
static int observe(struct dl_phdr_info*d,size_t size,void*context){
 UiImage*out=context;
 if(size<offsetof(struct dl_phdr_info,dlpi_phnum)+sizeof d->dlpi_phnum||!d->dlpi_name)return 0;
 if(*d->dlpi_name&&strcmp(d->dlpi_name,"/system/bin/camera-gui"))return 0;
 if(out->segments){out->count=129;return 1;}
 out->base=d->dlpi_addr;out->segments=d->dlpi_phdr;out->count=d->dlpi_phnum;return 0;
}
static int identity(void){
 char executable[128];ssize_t n=readlink("/proc/self/exe",executable,sizeof executable-1);
 if(n<=0||n>=(ssize_t)sizeof executable-1)return UI_ABI_PROCESS_REJECTED;
 executable[n]=0;if(strcmp(executable,"/system/bin/camera-gui"))return UI_ABI_PROCESS_REJECTED;
 /* This is a kernel-owned symlink to the executing inode, not a caller-selected link. */
 int fd=open("/proc/self/exe",O_RDONLY|O_CLOEXEC);if(fd<0)return UI_ABI_FILE_REJECTED;
 struct stat before={0},after={0};int valid=fstat(fd,&before)==0&&S_ISREG(before.st_mode)&&before.st_nlink==1&&before.st_size==GUI_FILE_SIZE;
 ReceiptSha sha;rsha_init(&sha);unsigned char buf[65536],digest[32];off_t bytes=0;
 while(valid){ssize_t got=read(fd,buf,sizeof buf);if(got<0&&errno==EINTR)continue;if(got<0){valid=0;break;}if(!got)break;bytes+=got;if(bytes>GUI_FILE_SIZE){valid=0;break;}rsha_update(&sha,buf,(unsigned)got);}
 if(fstat(fd,&after)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||before.st_size!=after.st_size||before.st_mtim.tv_sec!=after.st_mtim.tv_sec||before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec||before.st_ctim.tv_sec!=after.st_ctim.tv_sec||before.st_ctim.tv_nsec!=after.st_ctim.tv_nsec)valid=0;
 if(close(fd))valid=0;rsha_final(&sha,digest);
 return valid&&bytes==GUI_FILE_SIZE&&!memcmp(digest,gui_sha256,32)?UI_ABI_READY:UI_ABI_FILE_REJECTED;
}
X2D_UI_INTERNAL_API int x2d_ui_check_existing_runtime_v1(void){
 int rc=identity();if(rc)return rc;UiImage image={0};if(dl_iterate_phdr(observe,&image)||!image.segments)return UI_ABI_IMAGE_REJECTED;
 return ui_check_thread(&image,&gui_profile);
}
static UiImage verified_image;
static unsigned verified_image_state;
int x2d_ui_runtime_image(UiImage*out){
 if(!out)return UI_ABI_IMAGE_REJECTED;int rc=identity();if(rc)return rc;
 UiImage image={0};if(dl_iterate_phdr(observe,&image)||!image.segments)return UI_ABI_IMAGE_REJECTED;
 rc=ui_check_thread(&image,&gui_profile);if(rc)return rc;
 unsigned expected=0;if(__atomic_compare_exchange_n(&verified_image_state,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
  verified_image=image;__atomic_store_n(&verified_image_state,2,__ATOMIC_RELEASE);
 }
 *out=image;return 0;
}
/* A session has already checked the executing inode's full digest. Qt callbacks
 * use this mapping/thread check instead of hashing the executable every tick. */
int x2d_ui_recheck_image(const UiImage*expected){
 if(!expected)return UI_ABI_IMAGE_REJECTED;UiImage image={0};
 if(dl_iterate_phdr(observe,&image)||image.base!=expected->base||image.segments!=expected->segments||image.count!=expected->count)return UI_ABI_IMAGE_REJECTED;
 return ui_check_thread(&image,&gui_profile);
}
/* Destruction runs in the validated GUI session. Recheck mappings and GUI
 * thread, but never hash the 47 MB executable in each object's destructor. */
int x2d_ui_verified_image(UiImage*out){
 if(!out||__atomic_load_n(&verified_image_state,__ATOMIC_ACQUIRE)!=2)return UI_ABI_IMAGE_REJECTED;
 int rc=x2d_ui_recheck_image(&verified_image);if(rc)return rc;*out=verified_image;return 0;
}
int x2d_ui_schedule_image(const UiImage*image,int delay,UiSlotAction callback,UiSlotAction release,void*context){
 if(delay<0||delay>1000||!callback||!release)return 6;
 int rc=x2d_ui_recheck_image(image);if(rc)return rc;
 if(!ui_code(image,&gui_single_shot))return UI_ABI_IMAGE_REJECTED;
 void*app=NULL;memcpy(&app,(const void*)(image->base+gui_profile.application_self),sizeof app);if(!app)return UI_ABI_NO_APP;
 UiSlot*slot=ui_slot_new(callback,release,context);if(!slot)return 7;
 void(*schedule)(int,int,const void*,UiSlot*)=(void(*)(int,int,const void*,UiSlot*))(image->base+gui_single_shot.address);
 schedule(delay,0,app,slot);return 0;
}
int x2d_ui_register_pre(void(*callback)(void)){
 if(!callback)return 6;int rc=identity();if(rc)return rc;UiImage image={0};
 if(dl_iterate_phdr(observe,&image)||!image.segments)return UI_ABI_IMAGE_REJECTED;
 rc=ui_check_thread(&image,&gui_profile);if(rc!=UI_ABI_READY&&rc!=UI_ABI_NO_APP)return rc;
 if(!ui_code(&image,&gui_pre_routine))return UI_ABI_IMAGE_REJECTED;
 void(*add)(void(*)(void))=(void(*)(void(*)(void)))(image.base+gui_pre_routine.address);add(callback);return 0;
}
/* Explicit call only. The caller/library/context must remain resident until
 * release(context). Zero means Qt received a request, not that it executed.
 * Rejection retains context ownership with caller and never calls release.
 * Both callbacks must be noexcept. No constructor registers this entry.
 */
X2D_UI_INTERNAL_API int x2d_ui_post_once_v1(UiSlotAction callback,UiSlotAction release,void*context){
 if(!callback||!release)return 6;
 int rc=identity();if(rc)return rc;UiImage image={0};
 if(dl_iterate_phdr(observe,&image)||!image.segments)return UI_ABI_IMAGE_REJECTED;
 rc=ui_check_thread(&image,&gui_profile);if(rc)return rc;
 if(!ui_code(&image,&gui_single_shot))return UI_ABI_IMAGE_REJECTED;
 void*app=NULL;memcpy(&app,(const void*)(image.base+gui_profile.application_self),sizeof app);
 if(!app)return UI_ABI_NO_APP;
 UiSlot*slot=ui_slot_new(callback,release,context);if(!slot)return 7;
 void(*schedule)(int,int,const void*,UiSlot*)=(void(*)(int,int,const void*,UiSlot*))(image.base+gui_single_shot.address);
 schedule(0,0,app,slot);return 0;
}
