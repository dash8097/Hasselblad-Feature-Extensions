/* 在既有受保护的 render-probe 进程中消费合成行带。
 * 保留真实原厂 metadata、ION 同步、PID 租约和流互斥；不读取 merged RAW 文件。
 * 固定窗口分块的跨池容量和图像质量仍须机内验证，失败按通道 EOF 回到协调层取消。 */
#include "capture_band_wire.h"
#include "capture_render_band.h"
#include "factory_tile_copy.h"
#include "factory_memory_request.inc"
static int memory_listener=-1,memory_peer=-1,memory_socket_owned;
static int memory_native_submitted;
static CbwPacket memory_finish;
static int memory_transfer(int fd,void *data,uint64_t bytes,int sending){
 uint8_t *p=data;
 while(bytes){
  long n=sending?jpeg_sc(206,fd,(long)p,(long)bytes,0x4000,0,0):sc(63,fd,(long)p,(long)bytes,0);
  if(n==-4)continue;if(n<=0||(uint64_t)n>bytes)return 0;
  p+=n;bytes-=(uint64_t)n;
 }
 return 1;
}
static int memory_ack(const CbwPacket *request){
 CbwPacket ack=*request;ack.operation=CBW_ACK;
 return memory_transfer(memory_peer,&ack,sizeof ack,1);
}
static int memory_peer_identity(void){
 struct {int32_t pid;uint32_t uid,gid;} peer={0};uint32_t bytes=sizeof peer;
 if(jpeg_sc(209,memory_peer,1,17,(long)&peer,(long)&bytes,0)||
    bytes!=sizeof peer||peer.pid<=1||peer.uid)return 0;
 char path[64]="/proc/",digits[12],exe[128];unsigned n=6,k=0;uint32_t pid=(uint32_t)peer.pid;
 do{digits[k++]=(char)('0'+pid%10);pid/=10;}while(pid);
 while(k)path[n++]=digits[--k];
 const char *end="/exe";for(unsigned i=0;end[i];i++)path[n++]=end[i];path[n]=0;
 long length=sc(78,-100,(long)path,(long)exe,sizeof exe-1);
 if(length<=0||length>=(long)sizeof exe-1)return 0;exe[length]=0;
 return same(exe,"/system/bin/camera-storage");
}
static int memory_open(void){
 if(!cbw_platform())return 0;
 const char *endpoint=CBW_SOCKET_PATH;
 unsigned n=0;while(endpoint[n]&&n<108)n++;if(!n||n>=108)return 0;
 memory_listener=(int)sc(198,1,0x80001,0,0);if(memory_listener<0)return 0;
 struct {uint16_t family;char path[108];} address={.family=1};
 for(unsigned i=0;i<=n;i++)address.path[i]=endpoint[i];
 /* bind 拒绝既有路径；不删除未知 socket。目录由任务编排独占创建。 */
 if(sc(200,memory_listener,(long)&address,sizeof address,0))return 0;memory_socket_owned=1;
 if(sc(201,memory_listener,1,0,0))return 0;
 text("MEMORY_NATIVE_LISTENER_READY\n");
 struct {int32_t fd;int16_t events,revents;} pollfd={memory_listener,1,0};
 int64_t wait[2]={120,0};
 if(jpeg_sc(73,(long)&pollfd,1,(long)wait,0,8,0)!=1||!(pollfd.revents&1))return 0;
 memory_peer=(int)sc(242,memory_listener,0,0,0x80000);if(memory_peer<0)return 0;
 int64_t timeout[2]={120,0};
 if(jpeg_sc(208,memory_peer,1,20,(long)timeout,sizeof timeout,0)||
    jpeg_sc(208,memory_peer,1,21,(long)timeout,sizeof timeout,0))return 0;
 return memory_peer_identity();
}
/* 调用点在 repeat 已释放其 ION 引用、设备和模块之后。
 * FINISH 确认包含原厂编码、fsync、缓冲与会话收尾的实际结果。 */
static int memory_close(int success){
 int ok=success;
 if(memory_listener>=0&&sc(57,memory_listener,0,0,0))ok=0;memory_listener=-1;
 if(memory_socket_owned&&sc(35,-100,(long)CBW_SOCKET_PATH,0,0))ok=0;memory_socket_owned=0;
 if(memory_peer>=0){
  if(ok&&(!memory_finish.task||!memory_ack(&memory_finish)))ok=0;
  if(sc(57,memory_peer,0,0,0))ok=0;memory_peer=-1;
 }
 return ok;
}
#include "factory_memory_tiles.inc"
/* 无参考照片、无 Cache 回放、无普通存储 socket；显影输入全由已认证
 * camera-storage 内存通道交付。租约、互斥和 native stop 仍由原实现核对。 */
static int run_memory_native(void){
 void *bus=dbus_bus_get(1,0),*device=0;int rc=74,initialized=0,started=0,config=0;
 struct module modules[]={{"/dev/ion",duss_hal_attach_ion_mem,duss_hal_detach_ion_mem,0},
  {"/dev/ienc0",duss_hal_attach_verislcon_ienc,duss_hal_detach_verislcon_ienc,0},{0,0,0,0}};
 if(!bus)return rc;
 if(factory_native_group(bus)!=255||duss_hal_initialize(modules))goto done;initialized=1;
 if(duss_hal_device_open("/dev/ion",&config,&device)||duss_hal_device_start(device,&config))goto done;started=1;
 rc=render_memory_bands(bus,0,device);
done:
 if(memory_native_submitted&&!factory_stop_native(bus))rc=75;
 if(!factory_route_clear())rc=75;
 if(started&&duss_hal_device_stop(device,0))rc=75;
 if(device&&duss_hal_device_close(device))rc=75;
 if(initialized&&duss_hal_deinitialize())rc=75;
 if(!memory_close(rc==0)&&!rc)rc=73;
 dbus_connection_unref(bus);return rc;
}
