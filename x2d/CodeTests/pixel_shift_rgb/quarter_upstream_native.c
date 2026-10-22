/* 官方X2D 100C 4.2.0离线候选。显式初始化必须在拍摄线程启动前调用；
 * 无constructor、无轮询、无设备控制命令。匹配失败保留原厂编码。 */
#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <time.h>
#include <stdio.h>
#include "quarter_upstream.h"
#include "quarter_decision_wire.h"
#include "capture_ion_reference.h"
#include "quarter_upstream_bindings.inc"

static uintptr_t service,plugin;
static void (*captured_original)(void*,void*);
static void (*mutex_lock_internal)(void*),(*mutex_unlock_internal)(void*);
static void *(*container_get)(void*,uint32_t);
static int (*open_device)(const char*,void*,void**),(*close_device)(void*),(*device_state)(void*,uint8_t*);
static int (*allocate_memory)(void*,void**,uint32_t,uint32_t,uint32_t,uint32_t),(*free_memory)(void*);
static QduOps ops;static QduState state;
#ifdef QDU_PRELOAD
static int preload_ready;
#endif
static void *ion;
static pthread_mutex_t codec_mutex=PTHREAD_MUTEX_INITIALIZER,mapping_mutex=PTHREAD_MUTEX_INITIALIZER;
typedef struct {uint32_t engine,frame,exposure,token;uint64_t time;int valid;} Mapping;
static Mapping mappings[32];static unsigned cursor;
static uint32_t read32(const void*p,unsigned off){uint32_t n;memcpy(&n,(const char*)p+off,4);return n;}
static void *readptr(const void*p,unsigned off){void *n;memcpy(&n,(const char*)p+off,sizeof n);return n;}
static uint64_t now_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}

static void captured(void *engine,void *image){
 uint32_t native=0,frame=0,token=0;int raw=image&&read32(image,0x24)==QDU_RAW_KIND&&readptr(image,0x30);
 if(raw){
  native=read32(image,0x20);token=read32(image,0x28);frame=read32(readptr(image,0x30),0x20);
  pthread_mutex_lock(&mapping_mutex);
  for(unsigned i=0;i<32;i++)if(mappings[i].engine==native&&mappings[i].frame==frame)mappings[i].valid=0;
  pthread_mutex_unlock(&mapping_mutex);
 }
 captured_original(engine,image);
 if(!raw)return;
 /* 原回调已建立frame->CaptureRequest。使用同一原厂互斥锁读QMap。 */
 uintptr_t *lock=(uintptr_t*)((char*)engine+0x198),expected=0;
 if(!__atomic_compare_exchange_n(lock,&expected,1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))mutex_lock_internal(lock);
 uint32_t id=0;int found=qdu_exposure(engine,frame,service+QDU_FUNCTOR,&id);
 expected=1;
 if(!__atomic_compare_exchange_n(lock,&expected,0,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED))mutex_unlock_internal(lock);
 if(found){
  pthread_mutex_lock(&mapping_mutex);
  mappings[cursor++%32]=(Mapping){native,frame,id,token,now_ms(),1};
  pthread_mutex_unlock(&mapping_mutex);
 }
}
static int ask_storage(uint32_t exposure){
 int fd=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0);if(fd<0)return 0;
 struct sockaddr_un address={0};address.sun_family=AF_UNIX;
 memcpy(address.sun_path,QD_DECISION_SOCKET,sizeof QD_DECISION_SOCKET);
 int ok=0;
 if(connect(fd,(struct sockaddr*)&address,sizeof address))goto done;
 QdDecision q={QD_DECISION_MAGIC,exposure,0,0};
 if(send(fd,&q,sizeof q,MSG_NOSIGNAL)!=sizeof q)goto done;
 struct pollfd p={fd,POLLIN,0};
 if(poll(&p,1,10)>0&&recv(fd,&q,sizeof q,MSG_TRUNC)==sizeof q)ok=qd_decision_valid(&q,exposure);
done:close(fd);return ok;
}
static int eligible(void *context,const void *frame){
 (void)context;
 if(read32(frame,0x28)!=103||read32(frame,0x38)!=11656||read32(frame,0x3c)!=8742)return 0;
 void *meta=readptr(frame,0x218);if(!meta)return 0;
 const void *entry=container_get(meta,0xbb8);
 if(!entry||read32(entry,0)!=1||read32(entry,4)!=20||!readptr(entry,8))return 0;
 const void *data=readptr(entry,8);
 uint32_t engine=read32(data,0),token=read32(data,12),number=read32(frame,0x84),id=0;
 uint64_t time=now_ms();int found=0;
 pthread_mutex_lock(&mapping_mutex);
 for(unsigned i=0;i<32;i++)if(mappings[i].valid&&mappings[i].engine==engine&&mappings[i].frame==number&&mappings[i].token==token){
  Mapping *m=mappings+i;
  found=time&&m->time&&time>=m->time&&time-m->time<=10000;id=m->exposure;
  m->valid=0;break; /* 一个编码请求只消费一次；重复请求走原厂。 */
 }
 pthread_mutex_unlock(&mapping_mutex);
 return found&&ask_storage(id);
}
static int allocate(void *ctx,void **memory,uint32_t size){
 (void)ctx;
 if(!ion&&!cir_borrow(open_device,close_device,device_state,&ion))return -1;
 return allocate_memory(ion,memory,size,1024,23,0x10);
}
static int release(void *ctx,void *memory){(void)ctx;return free_memory(memory);}
#ifdef QDU_PRELOAD
static void encoding_metrics(uint64_t elapsed,int rc){
 char line[256];int n=snprintf(line,sizeof line,"{\"direct\":%u,\"fallback\":%u,\"failed\":%u,\"blocked\":%u,\"encode_total_ms\":%llu,\"rc\":%d}\n",
  state.direct,state.fallback,state.failed,state.blocked,(unsigned long long)elapsed,rc);
 if(n<=0||n>=(int)sizeof line)return;
 int fd=open("/tmp/x2d-quarter-upstream.metrics",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
 if(fd>=0){(void)write(fd,line,(size_t)n);close(fd);}
}
#endif
static int encode(void *encoder,QduRequest *request){
 if(pthread_mutex_trylock(&codec_mutex))return ops.encode(encoder,request);
#ifdef QDU_PRELOAD
 uint64_t start=now_ms();
#endif
 int rc=qdu_encode(&state,&ops,encoder,request);
 /* 错误返回可能仍有DMA：同时保留ION引用与唯一隔离缓冲。 */
 if(ion&&!state.quarantined){(void)close_device(ion);ion=0;}
#ifdef QDU_PRELOAD
 uint64_t end=now_ms();encoding_metrics(end>=start?end-start:0,rc);
#endif
 pthread_mutex_unlock(&codec_mutex);return rc;
}
static int bases(struct dl_phdr_info *im,size_t size,void *ctx){
 (void)size;(void)ctx;
 if(!im->dlpi_name[0]||!strcmp(im->dlpi_name,"/system/bin/camera-service"))service=im->dlpi_addr;
 if(strstr(im->dlpi_name,"/ienc/libdcam_ienc_jpeg.so"))plugin=im->dlpi_addr;
 return 0;
}
static int write_memory(uintptr_t target,const void *bytes,size_t count){
 int fd=open("/proc/self/mem",O_RDWR|O_CLOEXEC);if(fd<0)return 0;
 int ok=lseek(fd,(off_t)target,SEEK_SET)==(off_t)target&&write(fd,bytes,count)==(ssize_t)count;close(fd);
 __builtin___clear_cache((char*)target,(char*)target+count);
 return ok&&!memcmp((void*)target,bytes,count);
}
static void jump(void *out,uintptr_t target){uint32_t words[4]={0x58000051,0xd61f0220,0,0};memcpy(words+2,&target,8);memcpy(out,words,16);}
#ifdef QDU_PRELOAD
/* 冷启动捕获入口 + 动态链接器正常符号查找。不在编码线程写指令或GOT。 */
static const char *preload_stage="START";
static void preload_status(int ready){
 int fd=open("/tmp/x2d-quarter-upstream.status",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
 if(fd>=0){const char *s=ready?"READY_PRELOAD\n":"UNAVAILABLE\n";(void)write(fd,s,strlen(s));close(fd);}
 fd=open("/tmp/x2d-quarter-upstream.reason",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
 if(fd>=0){const char *s=ready?"READY":preload_stage;(void)write(fd,s,strlen(s));close(fd);}
}
typedef int (*OriginalEncode)(void*,QduRequest*);
static OriginalEncode original_encode;
int duss_hal_ienc_encfrm(void*,QduRequest*);
static int forward_encode(void *encoder,QduRequest *request){
 OriginalEncode fn=__atomic_load_n(&original_encode,__ATOMIC_ACQUIRE);
 if(!fn){
  fn=(OriginalEncode)dlsym(RTLD_NEXT,"duss_hal_ienc_encfrm");
  if(!fn||fn==duss_hal_ienc_encfrm){
   /* 原厂插件可能稍后以局部作用域加载。只借用已加载库，不主动装载编码驱动。 */
   void *handle=dlopen("/system/lib64/libduml_vcodec.so",RTLD_NOW|RTLD_NOLOAD);
   fn=handle?(OriginalEncode)dlsym(handle,"duss_hal_ienc_encfrm"):0;
   if(handle)dlclose(handle);
  }
  if(!fn||fn==duss_hal_ienc_encfrm){preload_stage="ENCODER_CALL_SYMBOL";preload_status(0);return -1001;}
  __atomic_store_n(&original_encode,fn,__ATOMIC_RELEASE);
 }
 return fn(encoder,request);
}
__attribute__((constructor)) static void qdu_preload_init(void){
 ops.encode=forward_encode;
 char exe[128];ssize_t n=readlink("/proc/self/exe",exe,sizeof exe-1);
 if(n<=0||n>=(ssize_t)sizeof exe-1)return;exe[n]=0;
 if(strcmp(exe,"/system/bin/camera-service"))return;
 dl_iterate_phdr(bases,0);
 preload_stage="SERVICE_FINGERPRINT";
 if(!service||memcmp((void*)(service+QDU_CAPTURE),qdu_capture_bytes,16)||
    memcmp((void*)(service+QDU_LOCK),qdu_lock_bytes,16)||
    memcmp((void*)(service+QDU_UNLOCK),qdu_unlock_bytes,16)){preload_status(0);return;}
#define PRELOAD_RESOLVE(field,name) do{preload_stage=name;field=(__typeof__(field))dlsym(RTLD_DEFAULT,name);if(!field){preload_status(0);return;}}while(0)
 PRELOAD_RESOLVE(ops.size,"duss_hal_mem_get_size");PRELOAD_RESOLVE(ops.map,"duss_hal_mem_map");
 PRELOAD_RESOLVE(ops.unmap,"duss_hal_mem_unmap");PRELOAD_RESOLVE(ops.sync,"duss_hal_mem_sync");
 PRELOAD_RESOLVE(container_get,"dcam_container_get_by_id");PRELOAD_RESOLVE(open_device,"duss_hal_device_open");
 PRELOAD_RESOLVE(close_device,"duss_hal_device_close");PRELOAD_RESOLVE(device_state,"duss_hal_device_get_state");
 PRELOAD_RESOLVE(allocate_memory,"duss_hal_mem_alloc");PRELOAD_RESOLVE(free_memory,"duss_hal_mem_free");
#undef PRELOAD_RESOLVE
 ops.eligible=eligible;ops.allocate=allocate;ops.release=release;
 mutex_lock_internal=(void(*)(void*))(service+QDU_LOCK);mutex_unlock_internal=(void(*)(void*))(service+QDU_UNLOCK);
 preload_stage="TRAMPOLINE_ALLOC";
 void *trampoline=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
 if(trampoline==MAP_FAILED){preload_status(0);return;}
 memcpy(trampoline,qdu_capture_bytes,16);jump((char*)trampoline+16,service+QDU_CAPTURE+16);
 __builtin___clear_cache(trampoline,(char*)trampoline+32);
 preload_stage="TRAMPOLINE_EXEC";
 if(mprotect(trampoline,4096,PROT_READ|PROT_EXEC)){munmap(trampoline,4096);preload_status(0);return;}
 captured_original=(void(*)(void*,void*))trampoline;
 uint8_t branch[16];jump(branch,(uintptr_t)captured);
 preload_stage="CAPTURE_WRITE";
 if(!write_memory(service+QDU_CAPTURE,branch,16)){
  /* 保留跳板，避免部分写入后释放仍可达的代码；启动失败不得标成READY。 */
  (void)write_memory(service+QDU_CAPTURE,qdu_capture_bytes,16);preload_status(0);return;
 }
 preload_ready=1;preload_status(1);
}
__attribute__((visibility("default"))) int duss_hal_ienc_encfrm(void *encoder,QduRequest *request){
 if(!ops.encode)return -1001;
 if(!preload_ready)return ops.encode(encoder,request);
 Dl_info origin={0};void *caller=__builtin_return_address(0);
 if(!dladdr(caller,&origin)||!origin.dli_fname||!strstr(origin.dli_fname,"/ienc/libdcam_ienc_jpeg.so")||
    (uintptr_t)caller!=(uintptr_t)origin.dli_fbase+0x1940||
    memcmp((char*)origin.dli_fbase+QDU_PLUGIN_ENCODE,qdu_plugin_bytes,16))return ops.encode(encoder,request);
 return encode(encoder,request);
}
#endif
/* 返回1已安装；0未安装；-1回退校验失败。调用者必须在拍摄线程启动前，
 * 且plugin已加载时调用。不得从活动拍摄线程热挂接或dlclose本模块。 */
__attribute__((visibility("default"))) int qdu_initialize_before_capture(void){
 static int attempted=0;if(attempted)return 0;attempted=1;
 char exe[128];ssize_t n=readlink("/proc/self/exe",exe,sizeof exe-1);
 if(n<=0||n>=(ssize_t)sizeof exe-1)return 0;exe[n]=0;
 if(strcmp(exe,"/system/bin/camera-service"))return 0;
 dl_iterate_phdr(bases,0);if(!service||!plugin)return 0;
 if(memcmp((void*)(service+QDU_CAPTURE),qdu_capture_bytes,16)||
    memcmp((void*)(plugin+QDU_PLUGIN_ENCODE),qdu_plugin_bytes,16)||
    memcmp((void*)(service+QDU_LOCK),qdu_lock_bytes,16)||
    memcmp((void*)(service+QDU_UNLOCK),qdu_unlock_bytes,16))return 0;
#define RESOLVE(field,name) do{field=(__typeof__(field))dlsym(RTLD_DEFAULT,name);if(!field)return 0;}while(0)
 RESOLVE(ops.encode,"duss_hal_ienc_encfrm");RESOLVE(ops.size,"duss_hal_mem_get_size");
 RESOLVE(ops.map,"duss_hal_mem_map");RESOLVE(ops.unmap,"duss_hal_mem_unmap");RESOLVE(ops.sync,"duss_hal_mem_sync");
 RESOLVE(container_get,"dcam_container_get_by_id");RESOLVE(open_device,"duss_hal_device_open");
 RESOLVE(close_device,"duss_hal_device_close");RESOLVE(device_state,"duss_hal_device_get_state");
 RESOLVE(allocate_memory,"duss_hal_mem_alloc");RESOLVE(free_memory,"duss_hal_mem_free");
 if(readptr((void*)plugin,QDU_HAL_GOT)!=(void*)ops.encode)return 0;
 ops.eligible=eligible;ops.allocate=allocate;ops.release=release;
 mutex_lock_internal=(void(*)(void*))(service+QDU_LOCK);mutex_unlock_internal=(void(*)(void*))(service+QDU_UNLOCK);
 void *trampoline=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
 if(trampoline==MAP_FAILED)return 0;
 memcpy(trampoline,qdu_capture_bytes,16);jump((char*)trampoline+16,service+QDU_CAPTURE+16);
 __builtin___clear_cache(trampoline,(char*)trampoline+32);
 if(mprotect(trampoline,4096,PROT_READ|PROT_EXEC)){munmap(trampoline,4096);return 0;}
 captured_original=(void(*)(void*,void*))trampoline;
 uint8_t branch[16];jump(branch,(uintptr_t)captured);uintptr_t wrapper=(uintptr_t)encode;
 if(write_memory(service+QDU_CAPTURE,branch,16)&&write_memory(plugin+QDU_HAL_GOT,&wrapper,8))return 1;
 uintptr_t original=(uintptr_t)ops.encode;
 int a=write_memory(service+QDU_CAPTURE,qdu_capture_bytes,16),b=write_memory(plugin+QDU_HAL_GOT,&original,8);
 /* 回退失败也不释放跳板；调用者必须保持库驻留并停止装载流程。 */
 return a&&b?0:-1;
}
