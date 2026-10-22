#include "factory_image_contract.h"
#include "factory_jpeg_layout.h"
/* 官方 X2D 4.2.0 camera-storage 的 ImageTransform::transform：
 * 0x15bf48 设置 w3=80，0x15bf58 调用 Ienc::encode。
 * 原厂 HAL 从请求 +0x10 读取质量；普通主图、预览与四亿分段统一沿用此值。 */
#ifndef FACTORY_JPEG_NATIVE_QUALITY
#define FACTORY_JPEG_NATIVE_QUALITY 80u
#endif
#ifndef FACTORY_JPEG_OUTPUT
#define FACTORY_JPEG_OUTPUT "/mnt/media_rw/ssd/x2d-factory-direct-100mp-01.jpg"
#endif
#ifndef FACTORY_JPEG_CAPACITY
#define FACTORY_JPEG_CAPACITY 67108864u
#endif
/* 固定 4.2.0 libduml_hal：IonFlag 的 bit 4 对应 cached + needs-sync。
 * 仅本任务新分配的缓冲使用缓存；写后 syncForDevice=2，读前 syncForCpu=1。
 * 不改变原厂持有的缓冲分配属性。 */
#define FACTORY_TASK_ION_FLAGS 0x10u
#define FACTORY_SYNC_DEVICE 2
#define FACTORY_SYNC_CPU 1
extern int duss_hal_initialize(void *);
extern int duss_hal_deinitialize(void);
extern int duss_hal_attach_ion_mem(const char *,void **);
extern int duss_hal_detach_ion_mem(void *);
extern int duss_hal_device_open(const char *,void *,void **);
extern int duss_hal_device_close(void *);
extern int duss_hal_device_start(void *,void *);
extern int duss_hal_device_stop(void *,void *);
extern int duss_hal_mem_import(void *,void **,uint64_t);
extern int duss_hal_mem_get_size(void *,uint32_t *);
extern int duss_hal_mem_map(void *,void **);
extern int duss_hal_mem_sync(void *,int);
extern int duss_hal_mem_unmap(void *);
extern int duss_hal_mem_free(void *);
extern int duss_hal_mem_alloc(void *,void **,uint32_t,uint32_t,uint32_t,uint32_t);
extern int duss_hal_attach_verislcon_ienc(const char *,void **);
extern int duss_hal_detach_verislcon_ienc(void *);
extern int duss_hal_ienc_encfrm(void *,void *);
static long jpeg_sc(long nr,long a,long b,long c,long d,long e,long f){
 register long x8 __asm__("x8")=nr,x0 __asm__("x0")=a,x1 __asm__("x1")=b,x2 __asm__("x2")=c;
 register long x3 __asm__("x3")=d,x4 __asm__("x4")=e,x5 __asm__("x5")=f;
 __asm__ volatile("svc 0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5):"memory","cc");return x0;
}
static void jpeg_msg(const char *s){unsigned n=0;while(s[n])n++;jpeg_sc(64,1,(long)s,n,0,0,0);}
static void jpeg_num(uint32_t v){char b[12];unsigned k=11;b[k]=0;do{b[--k]=(char)('0'+v%10);v/=10;}while(v);jpeg_msg(b+k);}
static uint64_t jpeg_monotonic_ms(void){
 int64_t stamp[2]={0,0};
 if(jpeg_sc(113,1,(long)stamp,0,0,0,0)||stamp[0]<0||stamp[1]<0||stamp[1]>=1000000000)return 0;
 return (uint64_t)stamp[0]*1000+(uint64_t)stamp[1]/1000000;
}
static void jpeg_elapsed(const char *phase,uint64_t begin,uint64_t end){
 if(!begin||end<begin||end-begin>UINT32_MAX)return;
 jpeg_msg(phase);jpeg_num((uint32_t)(end-begin));jpeg_msg("\n");
}
static int encode_jpeg_sized_to(void *ion,void *input,uint32_t width,uint32_t height,uint32_t stride,
 const char *path,uint32_t cap){
 void *output=0,*enc=0,*pixels=0;int rc=20,fd=-1,owned=0;
 if(!path||!cap)return rc;
 unsigned char frame[0x218],stream[0x1c8];
 if(duss_hal_mem_alloc(ion,&output,cap,1024,23,FACTORY_TASK_ION_FLAGS)||!output){jpeg_msg("NATIVE_JPEG_OUTPUT_ALLOCATE_REJECTED\n");goto end;}
 rc=21;if(duss_hal_device_open("/dev/ienc0",0,&enc)||!enc)goto end;
 if(factory_jpeg_layout_sized(frame,stream,input,output,cap,width,height,stride))goto end;
 struct {void *frame,*stream;uint32_t quality,reserved;} request={frame,stream,FACTORY_JPEG_NATIVE_QUALITY,0};
 jpeg_msg("NATIVE_JPEG_QUALITY value=");jpeg_num(request.quality);jpeg_msg("\n");
 jpeg_msg("NATIVE_JPEG_CAPACITY bytes=");jpeg_num(cap);jpeg_msg("\n");
 jpeg_msg("NATIVE_JPEG_BEGIN width=");jpeg_num(width);jpeg_msg(" height=");jpeg_num(height);jpeg_msg("\n");
 uint64_t encode_begin=jpeg_monotonic_ms();
 rc=22;int native=duss_hal_ienc_encfrm(enc,&request);
 jpeg_elapsed("NATIVE_JPEG_ENCODE_MS ",encode_begin,jpeg_monotonic_ms());
 if(native){jpeg_msg("ENCODE_RETURN ");if(native<0)jpeg_msg("-");jpeg_num(native<0?-(uint32_t)native:(uint32_t)native);jpeg_msg("\n");if(native==-1001)rc=31;goto end;}
 uint32_t offset=factory_u32(stream+0x3c),extra=factory_u32(stream+0x44),length=factory_u32(stream+0x40);
 rc=23;if(offset>cap||extra>cap-offset||length<4||length>cap-offset-extra)goto end;
 rc=24;if(duss_hal_mem_map(output,&pixels)||!pixels||duss_hal_mem_sync(output,FACTORY_SYNC_CPU))goto end;
 unsigned char *jpg=(unsigned char *)pixels+offset+extra;
 rc=25;if(jpg[0]!=255||jpg[1]!=216||jpg[length-2]!=255||jpg[length-1]!=217)goto end;
#ifdef FACTORY_JPEG_SYNTHETIC_VERIFY_ONLY
 /* 仅离线构建的自造图检查器启用；不把大测试图写进有限的 /data 分区。 */
 jpeg_msg("SYNTHETIC_JPEG_IN_MEMORY_VERIFIED bytes=");jpeg_num(length);jpeg_msg("\n");rc=0;goto end;
#endif
 /* Exclusive new output; never replace user photographs. No EXIF added yet. */
 rc=26;fd=(int)jpeg_sc(56,-100,(long)path,193,0600,0,0);if(fd<0)goto end;owned=1;
 uint64_t write_begin=jpeg_monotonic_ms();
 uint32_t done=0;rc=27;while(done<length){long k=jpeg_sc(64,fd,(long)(jpg+done),length-done,0,0,0);if(k==-4)continue;if(k<=0||k>(long)(length-done))goto end;done+=(uint32_t)k;}
 jpeg_elapsed("NATIVE_JPEG_WRITE_MS ",write_begin,jpeg_monotonic_ms());
 uint64_t flush_begin=jpeg_monotonic_ms();
 if(jpeg_sc(82,fd,0,0,0,0,0))goto end;
 jpeg_elapsed("NATIVE_JPEG_FSYNC_MS ",flush_begin,jpeg_monotonic_ms());
 jpeg_msg("NATIVE_JPEG_SAVED bytes=");jpeg_num(length);jpeg_msg("\n");rc=0;
end:
 if(fd>=0&&jpeg_sc(57,fd,0,0,0,0,0))rc=27;
 if(pixels && duss_hal_mem_unmap(output))rc=28;
 if(enc && duss_hal_device_close(enc))rc=29;
 if(output && duss_hal_mem_free(output))rc=30;
 /* 失败时仅移除本次独占创建的输出；既有文件绝不打开写入。 */
 if(rc&&owned)jpeg_sc(35,-100,(long)path,0,0,0,0);
 return rc;
}
#ifdef FACTORY_JPEG_RESTART_BANDS
#include "factory_jpeg_bands.inc"
#endif
static int encode_jpeg_sized(void *ion,void *input,uint32_t width,uint32_t height,uint32_t stride){
#ifdef FACTORY_JPEG_RESTART_BANDS
 if(width==23326&&height==17498&&stride==23552)
  return encode_jpeg_bands_to(ion,input,width,height,stride,FACTORY_JPEG_OUTPUT);
#endif
 int rc=encode_jpeg_sized_to(ion,input,width,height,stride,FACTORY_JPEG_OUTPUT,FACTORY_JPEG_CAPACITY);
 /* 4.2.0 的 -1001 包含输出溢出。仅完整四亿请求允许一次更大输出缓冲重试；
  * 前次编码器与输出已释放，没有创建成片，不重拍或重显影，不改变质量。 */
#ifndef FACTORY_JPEG_DISABLE_LARGER_RETRY
 if(rc==31&&width==23326&&height==17498&&FACTORY_JPEG_CAPACITY==268435456u){
  jpeg_msg("NATIVE_JPEG_LARGER_OUTPUT_RETRY_ONCE bytes=536870912\n");
  rc=encode_jpeg_sized_to(ion,input,width,height,stride,FACTORY_JPEG_OUTPUT,536870912u);
 }
#endif
 return rc;
}
#ifndef FACTORY_TARGET_408
static int encode_jpeg(void *ion,void *input){
 return encode_jpeg_sized(ion,input,FACTORY_IMAGE_WIDTH,FACTORY_IMAGE_HEIGHT,FACTORY_IMAGE_STRIDE);
}
#endif
