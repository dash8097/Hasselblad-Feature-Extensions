/* 原厂重显影请求的有限观察：仅键名/类型，不记录数值或共享句柄。
 * 正常 D-Bus AddMatch，不改变访问规则；拒绝时退出。 */
#include <stdint.h>
#ifndef FACTORY_MEMORY_BANDS
__attribute__((section(".interp"),used)) const char loader[]="/system/bin/linker64";
#endif
typedef struct {uint64_t opaque[16];} Iter;
extern void *dbus_bus_get(int,void*);
extern void *dbus_message_new_method_call(const char*,const char*,const char*,const char*);
extern int dbus_message_append_args(void*,int,...);
extern void *dbus_connection_send_with_reply_and_block(void*,void*,int,void*);
extern int dbus_message_get_type(void*);
extern int dbus_message_is_method_call(void*,const char*,const char*);
extern int dbus_connection_read_write(void*,int);
extern void *dbus_connection_pop_message(void*);
extern int dbus_message_iter_init(void*,Iter*);
extern int dbus_message_iter_get_arg_type(Iter*);
extern int dbus_message_iter_next(Iter*);
extern void dbus_message_iter_recurse(Iter*,Iter*);
extern void dbus_message_iter_get_basic(Iter*,void*);
extern void dbus_message_iter_get_fixed_array(Iter*,void*,int*);
extern void dbus_message_unref(void*);
extern void dbus_connection_close(void*);
extern void dbus_connection_unref(void*);
static long sc(long n,long a,long b,long c,long d){register long x8 __asm__("x8")=n,x0 __asm__("x0")=a,x1 __asm__("x1")=b,x2 __asm__("x2")=c,x3 __asm__("x3")=d;__asm__ volatile("svc 0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3):"memory","cc");return x0;}
static void text(const char*s){unsigned n=0;while(s[n]&&n<128)n++;sc(64,1,(long)s,n,0);}
static void number(unsigned v){char b[12];unsigned n=11;b[n]=0;do{b[--n]=(char)('0'+v%10);v/=10;}while(v);text(b+n);}
extern void *dbus_message_copy(void*);
extern const char *dbus_message_get_signature(void*);
extern const char *dbus_message_get_error_name(void*);
extern int dbus_message_set_sender(void*,const char*);
extern int dbus_message_set_destination(void*,const char*);
extern int duss_hal_initialize(void*);
extern int duss_hal_deinitialize(void);
extern int duss_hal_attach_ion_mem(const char*,void**);
extern int duss_hal_detach_ion_mem(void*);
extern int duss_hal_device_open(const char*,void*,void**);
extern int duss_hal_device_start(void*,void*);
extern int duss_hal_device_stop(void*,void*);
extern int duss_hal_device_close(void*);
extern int duss_hal_mem_import(void*,void**,uint64_t);
extern int duss_hal_mem_free(void*);
extern int duss_hal_mem_share(void*,uint64_t*);
extern int duss_hal_mem_get_phys_addr(void*,uint64_t*);
struct module{const char*name;int(*attach)(const char*,void**);int(*detach)(void*);void*handle;};
static int same(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static uint64_t handles[2];
static int schema(void*m){
 Iter args={0};if(!dbus_message_iter_init(m,&args))return 20;unsigned a=0;
 do{Iter map={0};if(dbus_message_iter_get_arg_type(&args)!='a')return 21;
 text("MAP ");number(a++);text("\n");dbus_message_iter_recurse(&args,&map);unsigned count=0;
 do{if(dbus_message_iter_get_arg_type(&map)==0)break;if(++count>64||dbus_message_iter_get_arg_type(&map)!='e')return 22;
 Iter pair={0},value={0};const char*key=0;dbus_message_iter_recurse(&map,&pair);
 if(dbus_message_iter_get_arg_type(&pair)!='s')return 23;dbus_message_iter_get_basic(&pair,&key);
 if(!key||!dbus_message_iter_next(&pair)||dbus_message_iter_get_arg_type(&pair)!='v')return 24;
 dbus_message_iter_recurse(&pair,&value);int typ=dbus_message_iter_get_arg_type(&value);
 if((a==1||a==3)&&same(key,"SHARE_HANDLE")){
 if(typ!='t')return 26;dbus_message_iter_get_basic(&value,&handles[a==1?0:1]);}
 text("KEY ");text(key);text(" TYPE ");number((unsigned)typ);text("\n");
 }while(dbus_message_iter_next(&map));
 }while(dbus_message_iter_next(&args));
 return a==3?0:25;
}
#ifdef FACTORY_RUNTIME_JOB
#include "factory_runtime_job.h"
static char runtime_jpeg_output[128],runtime_preview_output[128];
#undef FACTORY_JPEG_OUTPUT
#define FACTORY_JPEG_OUTPUT runtime_jpeg_output
#undef FACTORY_NATIVE_PREVIEW_OUTPUT
#define FACTORY_NATIVE_PREVIEW_OUTPUT runtime_preview_output
#endif
#include "factory_direct_codec.inc"
#ifdef FACTORY_INPROCESS_ROUTE
#include "factory_route_client.inc"
#endif
#ifdef FACTORY_REBUILD_REQUEST
#include "factory_request_clone.inc"
#endif
#ifdef FACTORY_TARGET_408
#include "factory_raw_stream.h"
static const char *candidate_raw_path;
static const char *candidate_ready_path;
/* 外部编排须在核对四亿配置、服务就绪与恢复机制后创建独立握手文件。
 * 本程序不修改配置或重启服务。握手初始必须不存在；120秒后退出。
 * 这仅是阶段同步信号，不是授权机制或硬件正确性的证明。 */
static int candidate_wait_ready(void){
#ifdef FACTORY_NATIVE_DISK400
 /* 完整磁盘合成已在启动前核对，不再等待旧的配置切换握手。 */
 text("NATIVE_DISK400_REFERENCE_READY\n");return 1;
#else
 text("NATIVE_METADATA_RETAINED_WAITING_FOR_408_CONFIGURATION\n");
 for(unsigned i=0;i<120;i++){
  int gate=(int)sc(56,-100,(long)candidate_ready_path,0,0);
  if(gate>=0){
   unsigned char b[5]={0};long n=sc(63,gate,(long)b,5,0);sc(57,gate,0,0,0);
   return n==4&&b[0]=='4'&&b[1]=='0'&&b[2]=='8'&&b[3]=='\n';
  }
  if(gate!=-2)return 0;
  long delay[2]={1,0};if(sc(101,(long)delay,0,0,0))return 0;
 }
 return 0;
#endif
}
static long candidate_read(void*context,unsigned char*dest,uint32_t n){
 long result;do{result=sc(63,*(int*)context,(long)dest,n,0);}while(result==-4);
 return result;
}
#endif
#ifdef FACTORY_TILED_408
#ifdef FACTORY_NATIVE_DISK400
#include "factory_disk400.inc"
#elif defined(FACTORY_MEMORY_BANDS)
#include "factory_memory_bands.inc"
#else
#include "factory_tiled_408.inc"
#endif
#endif
static int repeat(void*c,void*event,int *original_fd){
 int fd=*original_fd;
 void*dev=0,*input=0,*output=0,*copy=0,*reply=0;int config=0,rc=30,init=0,start=0;
 #ifdef FACTORY_NEW_BUFFERS
 void *fresh_in=0,*fresh_out=0,*source_map=0,*target_map=0;uint64_t fresh_handles[2]={0};
 #endif
 #ifdef FACTORY_TARGET_408
 FactoryBufferGeometry geometry[2];uint64_t actual_sizes[2]={0};int raw_fd=-1;
 #endif
 struct module modules[]={{"/dev/ion",duss_hal_attach_ion_mem,duss_hal_detach_ion_mem,0},{"/dev/ienc0",duss_hal_attach_verislcon_ienc,duss_hal_detach_verislcon_ienc,0},{0,0,0,0}};
 if(!handles[0]||!handles[1])return rc;
 if(duss_hal_initialize(modules))return rc;init=1;
 if(duss_hal_device_open("/dev/ion",&config,&dev))goto end;
 if(duss_hal_device_start(dev,&config))goto end;start=1;
 rc=31;if(duss_hal_mem_import(dev,&input,handles[0])||duss_hal_mem_import(dev,&output,handles[1]))goto end;
 text("BOTH_BUFFERS_RETAINED\n");
 /* Wait for the original render; keep its socket and imported buffer references alive. */

 unsigned char status[96];unsigned received=0;rc=32;
 while(received<56){long n=sc(63,fd,(long)(status+received),56-received,0);if(n==-4)continue;if(n<=0)goto end;received+=(unsigned)n;}
 if(!factory_full_yuv_header(status,56))goto end;
 while(received<96){long n=sc(63,fd,(long)(status+received),96-received,0);if(n==-4)continue;if(n<=0)goto end;received+=(unsigned)n;}
 text("ORIGINAL_RENDER_FINISHED\n");
 #ifdef FACTORY_REBUILD_REQUEST
 copy=rebuild_request(event,0);
 #else
 copy=dbus_message_copy(event);
 #endif
 rc=33;if(!copy)goto end;
 #ifdef FACTORY_NEW_BUFFERS
 #ifdef FACTORY_TARGET_408
 /* 原显影已返回；仅释放本进程导入引用，不声称其他缓存所有者已释放。 */
 rc=44;
 if(duss_hal_mem_free(input))goto end;input=0;
 if(duss_hal_mem_free(output))goto end;output=0;
 text("LOCAL_NATIVE_REFERENCES_RELEASED\n");
#ifdef FACTORY_INPROCESS_ROUTE
 /* 所需描述和元数据已复制完。关闭自己的参考连接，让存储服务
  * 释放参考会话；不假设共享内存回复还携带内联像素有效载荷。 */
 rc=49;
 if(sc(57,fd,0,0,0))goto end;*original_fd=-1;
 text("ORIGINAL_RENDER_SOCKET_CLOSED\n");
 rc=49;if(!factory_stop_native(c))goto end;
 text("NATIVE_REFERENCE_SESSION_STOPPED_WITHOUT_SERVICE_RESTART\n");
#endif
 rc=48;if(!candidate_wait_ready())goto end;
 #ifdef FACTORY_TILED_408
#ifdef FACTORY_NATIVE_DISK400
 rc=render_disk400(c,event,dev);goto end;
#elif defined(FACTORY_MEMORY_BANDS)
 rc=render_memory_bands(c,event,dev);goto end;
#else
 rc=render_tiled_408(c,event,dev);goto end;
#endif
 #endif
 if(!factory_buffer_geometry(&geometry[0],23326,17498,1)||
    !factory_buffer_geometry(&geometry[1],23326,17498,6))goto end;
 rc=45;
 if(duss_hal_mem_alloc(dev,&fresh_in,geometry[0].allocation_bytes,1024,23,0)||
    duss_hal_mem_alloc(dev,&fresh_out,geometry[1].allocation_bytes,1024,23,0))goto end;
 uint32_t actual_in=0,actual_out=0;
 if(duss_hal_mem_get_size(fresh_in,&actual_in)||duss_hal_mem_get_size(fresh_out,&actual_out))goto end;
 actual_sizes[0]=actual_in;actual_sizes[1]=actual_out;
 if(!factory_buffer_fits(&geometry[0],actual_in)||!factory_buffer_fits(&geometry[1],actual_out))goto end;
 rc=46;
 if(duss_hal_mem_map(fresh_in,&target_map)||!target_map)goto end;
 raw_fd=(int)sc(56,-100,(long)candidate_raw_path,0,0);if(raw_fd<0)goto end;
 if(!factory_raw_stream(candidate_read,&raw_fd,target_map,geometry[0].content_bytes,actual_in))goto end;
 if(sc(57,raw_fd,0,0,0)){raw_fd=-1;goto end;}raw_fd=-1;
 if(duss_hal_mem_sync(fresh_in,0))goto end;
 if(duss_hal_mem_unmap(fresh_in))goto end;target_map=0;
 rc=47;
 if(duss_hal_mem_share(fresh_in,&fresh_handles[0])||duss_hal_mem_share(fresh_out,&fresh_handles[1]))goto end;
 dbus_message_unref(copy);copy=rebuild_request_sized(event,fresh_handles,geometry,actual_sizes);
 if(!copy)goto end;
 text("CANDIDATE_408_REQUEST_READY_CALIBRATION_UNVERIFIED\n");
 #else
 /* 原尺寸对照：仅替换缓冲所有权；保持全部像素、描述、元数据不变。 */
 uint32_t in_size=0,out_size=0;rc=40;
 if(duss_hal_mem_get_size(input,&in_size)||duss_hal_mem_get_size(output,&out_size))goto end;
 if(!in_size||in_size>268435456u||out_size<FACTORY_IMAGE_BYTES||out_size>268435456u)goto end;
 if(duss_hal_mem_alloc(dev,&fresh_in,in_size,1024,23,0)||duss_hal_mem_alloc(dev,&fresh_out,out_size,1024,23,0))goto end;
 rc=41;if(duss_hal_mem_map(input,&source_map)||!source_map||duss_hal_mem_sync(input,1))goto end;
 if(duss_hal_mem_map(fresh_in,&target_map)||!target_map)goto end;
 for(uint32_t i=0;i<in_size;i++)((unsigned char*)target_map)[i]=((unsigned char*)source_map)[i];
 if(duss_hal_mem_sync(fresh_in,0))goto end;
 if(duss_hal_mem_unmap(input))goto end;source_map=0;
 if(duss_hal_mem_unmap(fresh_in))goto end;target_map=0;
 rc=42;if(duss_hal_mem_share(fresh_in,&fresh_handles[0])||duss_hal_mem_share(fresh_out,&fresh_handles[1]))goto end;
 dbus_message_unref(copy);copy=rebuild_request(event,fresh_handles);if(!copy)goto end;
 text("NEW_BUFFERS_SHARED\n");
 #endif
 #endif
 if(!dbus_message_set_sender(copy,0)||!dbus_message_set_destination(copy,"com.hasselblad.camera"))goto end;
 text("DIRECT_REPROCESS_SUBMIT\n");reply=dbus_connection_send_with_reply_and_block(c,copy,20000,0);rc=34;
 if(!reply){text("DIRECT_NO_REPLY\n");goto end;}
 text("DIRECT_REPLY_TYPE ");number(dbus_message_get_type(reply));text(" SIGNATURE ");text(dbus_message_get_signature(reply));text("\n");
 if(dbus_message_get_type(reply)!=2){const char*e=dbus_message_get_error_name(reply);if(e)text(e);text("\n");goto end;}
 Iter result={0};if(!dbus_message_iter_init(reply,&result)){text("DIRECT_EMPTY_RETURN\n");
 #ifdef FACTORY_TARGET_408
 rc=encode_jpeg_sized(dev,fresh_out,geometry[1].width,geometry[1].height,geometry[1].stride_pixels);
 #elif defined(FACTORY_NEW_BUFFERS)
 rc=encode_jpeg(dev,fresh_out);
 #else
 rc=encode_jpeg(dev,output);
 #endif
 goto end;}
 int typ=dbus_message_iter_get_arg_type(&result);int32_t value=-1;
 if(typ!='i'&&typ!='u'&&typ!='b')goto end;dbus_message_iter_get_basic(&result,&value);
 text("UNEXPECTED_DIRECT_RETURN_TYPE ");number((unsigned)typ);text("\n");
 /* 实机确认成功为无参数返回；非空回复不可冒充已编码成功。 */
 rc=34;
end:
#ifdef FACTORY_INPROCESS_ROUTE
 if(!factory_route_clear()||!factory_stop_native(c))rc=49;
#endif
 #ifdef FACTORY_TARGET_408
 if(raw_fd>=0)sc(57,raw_fd,0,0,0);
 #endif
 if(reply)dbus_message_unref(reply);if(copy)dbus_message_unref(copy);
 #ifdef FACTORY_NEW_BUFFERS
 if(source_map&&duss_hal_mem_unmap(input))rc=43;if(target_map&&duss_hal_mem_unmap(fresh_in))rc=43;
 if(fresh_out&&duss_hal_mem_free(fresh_out))rc=43;if(fresh_in&&duss_hal_mem_free(fresh_in))rc=43;
 #endif
 if(output&&duss_hal_mem_free(output))rc=35;if(input&&duss_hal_mem_free(input))rc=35;
 if(start&&duss_hal_device_stop(dev,0))rc=36;if(dev&&duss_hal_device_close(dev))rc=36;
 if(init&&duss_hal_deinitialize())rc=36;
#if defined(FACTORY_MEMORY_BANDS) && !defined(FACTORY_NATIVE_DISK400)
 if(!memory_close(rc==0))rc=73;
#endif
 return rc;
}
#ifdef FACTORY_MEMORY_BANDS
/* 旧回放代码仅供旧构建，内存入口不调用；优化构建会移除整条不可达链路。 */
__attribute__((unused))
#endif
static int run(const char*path){
 unsigned n=0;while(path[n]&&n<1025){if(path[n]=='.'&&path[n+1]=='.')return 1;n++;}if(n<5||n>1024)return 1;
#if defined(FACTORY_MEMORY_BANDS) && !defined(FACTORY_NATIVE_DISK400)
 /* 只请求当前内存任务刚写完的首帧；不得使用其他既有照片取 metadata。 */
 unsigned parent=0;while(candidate_raw_path[parent]&&parent<108)parent++;
 if(parent<12||!same(candidate_raw_path+parent-12,"/memory.sock"))return 1;
 parent-=12;
 for(unsigned i=0;i<parent;i++)if(path[i]!=candidate_raw_path[i])return 1;
 if(!same(path+parent,"/frame-0.3FR"))return 1;
#endif
 text("OBSERVER_BEGIN\n");
 void*c=dbus_bus_get(1,0),*m=0,*r=0;int rc=2,fd=-1;if(!c)return rc;text("BUS_CONNECTED\n");
 const char*rule="type='method_call',sender='com.hasselblad.storage',interface='com.hasselblad.camera',member='reprocess_exposure_item',eavesdrop='true'";
 m=dbus_message_new_method_call("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","AddMatch");
 if(!m||!dbus_message_append_args(m,'s',&rule,0))goto end;
 text("MATCH_BUILT\n");
 r=dbus_connection_send_with_reply_and_block(c,m,3000,0);rc=3;if(!r||dbus_message_get_type(r)!=2){text("MATCH_NOT_ALLOWED\n");goto end;}
 text("MATCH_ACCEPTED\n");dbus_message_unref(r);r=0;
 fd=(int)sc(198,1,1,0,0);rc=4;if(fd<0)goto end;
 struct{uint16_t family;char path[108];}sa={1,"/tmp/storage-socket"};
 if(sc(203,fd,(long)&sa,sizeof sa,0))goto end;
 #ifdef FACTORY_TARGET_408
 long receive_timeout[2]={30,0};
 if(jpeg_sc(208,fd,1,20,(long)receive_timeout,sizeof receive_timeout,0))goto end;
 #endif
 unsigned char request[1043]={0};request[0]=(n+17)&255;request[1]=(n+17)>>8;request[2]=2;
 for(unsigned i=0;i<n;i++)request[19+i]=path[i];
 rc=5;if(sc(64,fd,(long)request,n+19,0)!=(long)(n+19))goto end;
 /* At most 20 seconds, one existing-photo reprocess request, no retry. */
 rc=6;
 for(unsigned k=0;k<200;k++){
  if(!dbus_connection_read_write(c,100))break;
  void*event;while((event=dbus_connection_pop_message(c))){
   if(dbus_message_is_method_call(event,"com.hasselblad.camera","reprocess_exposure_item")){rc=schema(event);if(!rc)rc=repeat(c,event,&fd);dbus_message_unref(event);goto end;}
   dbus_message_unref(event);
  }
 }
end:
 if(fd>=0)sc(57,fd,0,0,0);if(r)dbus_message_unref(r);if(m)dbus_message_unref(m);
 dbus_connection_unref(c);return rc;
}
#ifdef FACTORY_RAW_STACK
#include "factory_raw_stack.inc"
#endif
#ifdef FACTORY_SIXTEEN_DISK
#include "factory_sixteen_disk.inc"
#endif
__attribute__((noreturn,used))void entry(long*s){
 int r=1;
 #ifdef FACTORY_TARGET_408
 /* 三个显式输入：原厂参考照片、独立准备的完整填充RAW、阶段握手路径。
  * 不把3FR或DNG容器误当裸数据；长度与尾部检查另在读取阶段执行。 */
 if(s[0]==4){candidate_raw_path=(const char*)s[3];candidate_ready_path=(const char*)s[4];
 #ifdef FACTORY_RUNTIME_JOB
 if(!factory_runtime_paths(candidate_raw_path,candidate_ready_path,runtime_jpeg_output,runtime_preview_output)){
  text("RUNTIME_JOB_PATH_REJECTED\n");sc(94,1,0,0,0);__builtin_unreachable();
 }
 #endif
  unsigned n=0;while(candidate_raw_path[n]&&n<1025)n++;
  unsigned k=0;while(candidate_ready_path[k]&&k<1025)k++;
  if(n>4&&n<=1024&&candidate_raw_path[0]=='/'&&
#if defined(FACTORY_NATIVE_DISK400)
     same(candidate_raw_path+n-4,".dng")&&
#elif defined(FACTORY_MEMORY_BANDS)
     same(candidate_raw_path+n-5,".sock")&&
#elif defined(FACTORY_PACKED_DNG_INPUT)
     same(candidate_raw_path+n-4,".dng")&&
#else
     same(candidate_raw_path+n-4,".raw")&&
#endif
     k>6&&k<=1024&&candidate_ready_path[0]=='/'&&same(candidate_ready_path+k-6,".ready")){
   int exists=(int)sc(56,-100,(long)candidate_ready_path,0,0);
   if(exists>=0)sc(57,exists,0,0,0);
   else if(exists==-2){
#if defined(FACTORY_MEMORY_BANDS) && !defined(FACTORY_NATIVE_DISK400)
    r=run_memory_native();
#else
    r=run((const char*)s[2]);
#endif
   }}}
 #else
 r=s[0]==2?run((const char*)s[2]):1;
 #endif
 text("OBSERVER_EXIT ");number((unsigned)r);text("\n");sc(94,r,0,0,0);__builtin_unreachable();}
#ifdef FACTORY_MEMORY_BANDS
/* 与已通过的隔离 Bionic 检查器使用同一启动约定，先初始化 libc，
 * 再进入参数验证；参数不合法时不创建总线或设备会话。 */
typedef struct {void (**preinit)(void),(**init)(void),(**fini)(void);} MemoryStructors;
extern __attribute__((noreturn))void __libc_init(void *,void (*)(void),int (*)(int,char**,char**),const MemoryStructors *);
static int memory_main(int argc,char **argv,char **envp){
#ifdef FACTORY_QUARTER_SINGLE
 if(argc==3&&same(argv[1],"--quarter-single"))return run_raw_stack(argv[2]);
#ifdef FACTORY_QUARTER_MEMORY
 if(argc==3&&same(argv[1],"--quarter-handoff-check")){quarter_handoff_check=1;return run_raw_stack(argv[2]);}
#endif
 (void)envp;return 1;
#endif
#ifdef FACTORY_SIXTEEN_DISK
 if(argc==3&&same(argv[1],"--sixteen-disk"))return run_sixteen_disk(argv[2]);
 /* 独立十六張程序不能误走六帧或普通堆栈入口。 */
 (void)envp;return 1;
#endif
#ifdef FACTORY_RAW_STACK
 if(argc==3&&same(argv[1],"--raw-stack"))return run_raw_stack(argv[2]);
#endif
#ifdef FACTORY_JPEG_REFERENCE_SHARPEN
 if(argc==2&&same(argv[1],"--sharpen-check")){
  /* 小型自造亮度/色度；使用真实 Bionic 四线程，不创建图像池或相机会话。 */
  uint8_t pixels[33*19*2],expected[33*19*2];
  for(unsigned i=0;i<sizeof pixels;i++)pixels[i]=expected[i]=(uint8_t)((i*71u+(i/33)*17u)&255);
  for(unsigned y=2;y<17;y++)for(unsigned x=2;x<31;x++){
   int sum=0;const unsigned weight[5]={1,1,4,1,1};
   for(unsigned dy=0;dy<5;dy++)for(unsigned dx=0;dx<5;dx++)sum+=weight[dy]*weight[dx]*pixels[(y+dy-2)*33+x+dx-2];
   expected[y*33+x]=(uint8_t)fs_value(pixels[y*33+x],sum);
  }
  if(!factory_reference_sharpen(pixels,33,19,33))return 76;
  for(unsigned i=0;i<sizeof pixels;i++)if(pixels[i]!=expected[i])return 76;
  text("JPEG_SHARPEN_FOUR_THREADS_SYNTHETIC_PASSED_NO_CAMERA_SESSION\n");return 0;
 }
#endif
 (void)envp;long stack[6]={(long)argc,0,0,0,0,0};
 for(unsigned i=0;i<4&&i<(unsigned)argc;i++)stack[i+1]=(long)argv[i];
 entry(stack);
}
__attribute__((noreturn,used))void memory_entry_point(void *stack){
 const MemoryStructors empty={0};__libc_init(stack,0,memory_main,&empty);
}
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0, sp\nbl memory_entry_point");}
#else
__attribute__((naked,noreturn))void _start(void){__asm__ volatile("mov x0, sp\nbl entry");}
#endif
