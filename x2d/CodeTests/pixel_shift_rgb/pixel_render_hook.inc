/* 原厂 RawPb 事件仅针对有效私有租约的两块缓冲选择既有 stream group 11。
 * 入口和 ABI 绑定官方 4.2.0；不改变配置、标定、凭据或普通回放选路。 */
#include "pixel_render_route.h"
#include "render_fingerprint.h"
/* basic_string_view 按 AArch64 ABI 分成地址和长度；其余参数原样转发。 */
typedef void (*PoRenderPost)(void*,void*,const char*,U,int);
static PoRenderPost route_original;
static int route_read_lease(PoRenderLease *lease){
 char data[sizeof *lease+1],proc[64],stat[2048],exe[128];
 int n=read_regular(CHANNEL "render-lease",data,sizeof data);if(n!=(int)sizeof *lease)return 0;
 memcpy(lease,data,sizeof *lease);
 if(lease->pid<2||lease->pid>4194304)return 0;
 snprintf(proc,sizeof proc,"/proc/%u/exe",lease->pid);
 long z=readlink(proc,exe,sizeof exe-1);if(z<1||z>=(long)sizeof exe)return 0;exe[z]=0;
 if(!po_render_executable_allowed(exe))return 0;
 snprintf(proc,sizeof proc,"/proc/%u/stat",lease->pid);
 /* procfs 的大小为零，不能用要求 size==read 的普通文件读取器。 */
 int fd=open(proc,0xA0000);if(fd<0)return 0;
 z=read(fd,stat,sizeof stat-1);close(fd);if(z<=0||z==(long)sizeof stat-1)return 0;
 uint64_t ticks=0;if(!po_render_start_ticks(stat,(U)z,&ticks))return 0;
 return po_render_lease_valid(lease,sizeof *lease,now(),1,ticks);
}
static int route_ack(const PoRenderLease *lease){
 const char *temp=CHANNEL "render-accepted.tmp";
 int fd=open(temp,0xA00C1,0600);if(fd<0)return 0;
 int ok=write(fd,lease,sizeof *lease)==sizeof *lease;
 if(close(fd))ok=0;
 if(ok&&!rename(temp,CHANNEL "render-accepted"))return 1;
 (void)syscall(35,-100,temp,0);return 0;
}
static void route_post(void *self,void *event,const char *source,U length,int line){
 PoRenderLease lease;
 /* 仅在原厂构造完成、尚未投递的 RawPbReprocessEvent 上调整分支字节。 */
 if(event&&*(U*)event==base+0x7a5ab0&&*(uint32_t*)((char*)event+0x68)==15&&
    *((unsigned char*)event+0x6c)==12&&route_read_lease(&lease)){
  put(CHANNEL "render-dispatch.tmp",CHANNEL "render-dispatch.status","PRIVATE_RAWPB_EVENT_SEEN\n");
  void *in=*(void**)((char*)event+0x70),*out=*(void**)((char*)event+0x80);
  if(in&&out&&*(U*)in==base+0x7a8b40&&*(U*)out==base+0x7a8b40){
   /* rawPhys 使用已有 DussMemShare 的正常 HAL 查询，不再次导出句柄。
    * 当前任务从分配起点开始，拒绝带非零偏移的子缓冲。 */
   uint64_t (*identity)(void*)=(uint64_t(*)(void*))(base+0x24a518);
   unsigned chosen=*(uint64_t*)((char*)in+0x68)||*(uint64_t*)((char*)out+0x68)?12:
     po_render_group(&lease,12,identity(in),identity(out));
   if(chosen==11&&route_ack(&lease)){
    *((unsigned char*)event+0x6c)=11;
    put(CHANNEL "render-dispatch.tmp",CHANNEL "render-dispatch.status","PRIVATE_RAWPB_GROUP_11_ACCEPTED\n");
   }else put(CHANNEL "render-dispatch.tmp",CHANNEL "render-dispatch.status",
       chosen==11?"PRIVATE_RAWPB_ACK_WRITE_REJECTED\n":"PRIVATE_RAWPB_BUFFER_IDENTITY_REJECTED\n");
  }else put(CHANNEL "render-dispatch.tmp",CHANNEL "render-dispatch.status","PRIVATE_RAWPB_BUFFER_LAYOUT_REJECTED\n");
 }
 route_original(self,event,source,length,line);
}
static int route_install(void){
 for(unsigned i=0;i<sizeof render_checks/sizeof render_checks[0];i++)
  if(memcmp((void*)(base+render_checks[i].offset),render_checks[i].bytes,render_checks[i].length))return 0;
 U *slot=(U*)(base+0x7a2678),expected=base+0x107690;
 if(*slot!=expected)return 0;
 route_original=(PoRenderPost)expected;
 U page=(U)slot&~(U)4095;
 if(mprotect((void*)page,4096,3))return 0;
 __atomic_store_n(slot,(U)route_post,__ATOMIC_RELEASE);
 if(mprotect((void*)page,4096,1)){
  __atomic_store_n(slot,expected,__ATOMIC_RELEASE);
  (void)mprotect((void*)page,4096,1);return 0;
 }
 return 1;
}
