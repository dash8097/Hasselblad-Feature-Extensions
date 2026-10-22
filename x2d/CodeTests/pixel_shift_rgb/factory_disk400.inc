/* X2D 4.2.0：只消费已写完的六帧合成 DNG。拍摄期不持有原厂 RAW。
 * 九块串行原厂显影；释放 RAW 后编码当前块，JPEG 块写 SSD。
 * 每块有 8px 外围用于锐化，裁切后直接拼接压缩数据，不分配整幅 YUV。 */
#include "factory_native_3fr.h"
#include "factory_raw_stack_diagnostics.h"
#ifndef FACTORY_DISK400_HOST_CHECK
#include "factory_raw_tile.h"
static __attribute__((noreturn)) void factory_sharpen_join_failure(void){
 text("DISK400_SHARPEN_JOIN_FAILED\n");sc(94,76,0,0,0);__builtin_unreachable();
}
#include "factory_reference_sharpen.h"
extern void *memmove(void *,const void *,__SIZE_TYPE__);
#endif
#ifndef D400_WIDTH
#define D400_WIDTH 23326u
#define D400_HEIGHT 17498u
#define D400_STEP_X 11632u
#define D400_STEP_Y 8720u
#define D400_HALO 8u
#endif
#define D400_JPEG_CAPACITY (64u*1024u*1024u)
typedef struct {int fd;} Disk400Input;
static int disk400_read(void *context,uint64_t off,unsigned char *out,uint32_t bytes){
 Disk400Input *in=context;
 const uint64_t size=(uint64_t)D400_WIDTH*D400_HEIGHT*2;
 if(off>size||bytes>size-off)return 0;
 while(bytes){long n=jpeg_sc(67,in->fd,(long)out,bytes,(long)(4096+off),0,0);
  if(n==-4)continue;if(n<=0||(uint64_t)n>bytes)return 0;
  out+=n;off+=(uint64_t)n;bytes-=(uint32_t)n;
 }
 return 1;
}
static int disk400_path(char out[192],const char *leaf){
 unsigned n=0;while(candidate_raw_path[n]&&n<128)n++;
 if(n<11||n>=128||!same(candidate_raw_path+n-11,"/merged.dng"))return 0;
 unsigned parent=n-10;for(unsigned i=0;i<parent;i++)out[i]=candidate_raw_path[i];
 for(unsigned i=0;;i++){if(parent+i>=192)return 0;out[parent+i]=leaf[i];if(!leaf[i])break;}
 return 1;
}
static int disk400_tile_path(char out[192],unsigned tile){
 if(tile>8)return 0;char leaf[]="render-tile-0.jpg";leaf[12]=(char)('0'+tile);
 return disk400_path(out,leaf);
}
static int disk400_progress(unsigned phase,unsigned done,unsigned encoded){
 if(phase>3||done>9||encoded>done)return 0;
 char path[192],tmp[192],line[]="0 0 0\n";
 if(!disk400_path(path,"disk400.progress")||!disk400_path(tmp,"disk400.progress.tmp"))return 0;
 line[0]=(char)('0'+phase);line[2]=(char)('0'+done);line[4]=(char)('0'+encoded);
 int fd=(int)sc(56,-100,(long)tmp,0xA0241,0600);if(fd<0)return 0;
 int ok=sc(64,fd,(long)line,6,0)==6;
 if(sc(57,fd,0,0,0))ok=0;
 return ok&&!sc(38,-100,(long)tmp,-100,(long)path);
}
static int disk400_release(void **buffer){
 if(!*buffer)return 1;
 if(duss_hal_mem_free(*buffer))return 0;
 *buffer=0;return 1;
}
static int disk400_allocate(void *ion,void **raw,void **yuv,const FactoryBufferGeometry *g){
 /* 两个工作块都到位才提交显影。重试前先释放已经分配的部分，
  * 不累积持有缓冲，不重发曝光，不重启原厂服务。最多等待 10 秒。 */
 for(unsigned attempt=0;attempt<100;attempt++){
  int a=duss_hal_mem_alloc(ion,raw,g[0].allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS);
  int b=(!a&&*raw)?duss_hal_mem_alloc(ion,yuv,g[1].allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS):-1;
  if(!a&&!b&&*raw&&*yuv)return 1;
  if(!disk400_release(raw)||!disk400_release(yuv))return 0;
  if(!attempt)text("DISK400_WAITING_RENDER_WORKSPACE\n");
  long delay[2]={0,100000000};if(sc(101,(long)delay,0,0,0))return 0;
 }
 text("DISK400_WORKSPACE_UNAVAILABLE_INPUTS_RETAINED\n");return 0;
}
static int render_disk400(void *bus,void *event,void *ion){
 int rc=86,fd=-1,submitted=0;unsigned tile=0;const char *phase="INPUT";
 void *raw=0,*yuv=0,*map=0,*request=0,*reply=0;
 FactoryBufferGeometry g[2];uint64_t handles[2]={0},identities[2]={0},sizes[2];
 uint32_t actual[2];uint8_t header[4096];uint16_t cache0[11836],cache1[11836];char path[192];
 if(!event||factory_native_group(bus)!=255)goto done;
 fd=(int)sc(56,-100,(long)candidate_raw_path,0xA0000,0);
 if(fd<0||jpeg_sc(67,fd,(long)header,sizeof header,0,0,0)!=sizeof header||
    sc(62,fd,0,2,0)!=4096+(long)D400_WIDTH*D400_HEIGHT*2)goto done;
 uint32_t ifd=f3_u32(header+4),white=0,black=0;
 if(memcmp(header,"II*\0",4)||!f3_is(header,sizeof header,ifd,256,D400_WIDTH)||
    !f3_is(header,sizeof header,ifd,257,D400_HEIGHT)||!f3_is(header,sizeof header,ifd,258,16)||
    !f3_is(header,sizeof header,ifd,259,1)||!f3_is(header,sizeof header,ifd,273,4096)||
    !f3_is(header,sizeof header,ifd,279,D400_WIDTH*D400_HEIGHT*2)||
    !f3_scalar(header,sizeof header,ifd,50717,&white)||!f3_scalar(header,sizeof header,ifd,50714,&black)||
    white<=black||white>65535)goto done;
 uint8_t *cfa=f3_data(header,sizeof header,f3_tag(header,sizeof header,ifd,33422),1,4,1);
 const uint8_t grbg[4]={1,0,2,1};if(!cfa||memcmp(cfa,grbg,4))goto done;
 for(unsigned i=0;i<9;i++){
  if(!disk400_tile_path(path,i))goto done;
  long exists=sc(56,-100,(long)path,0xA0000,0);
  if(exists>=0){sc(57,exists,0,0,0);goto done;}if(exists!=-2)goto done;
 }
 if(!factory_buffer_geometry(g,11836,8842,1)||!factory_buffer_geometry(g+1,11656,8742,6))goto done;
 Disk400Input image={fd};
 text("DISK400_NATIVE_CAPTURE_REFERENCES_HELD 0\n");
 text("DISK400_WORKSPACE_PEAK_BYTES ");number(g[0].allocation_bytes+g[1].allocation_bytes);text("\n");
 for(unsigned oy=0;oy<D400_HEIGHT;oy+=D400_STEP_Y)for(unsigned ox=0;ox<D400_WIDTH;ox+=D400_STEP_X){
  if(!disk400_progress(1,tile,tile))goto done;
  phase="ALLOCATE";if(!disk400_allocate(ion,&raw,&yuv,g))goto done;
  if(duss_hal_mem_get_size(raw,actual)||duss_hal_mem_get_size(yuv,actual+1)||
     !factory_buffer_fits(g,actual[0])||!factory_buffer_fits(g+1,actual[1]))goto done;
  phase="RAW_READ";uint64_t begin=jpeg_monotonic_ms();
  if(duss_hal_mem_map(raw,&map)||!map||!factory_raw_tile(disk400_read,&image,map,
     D400_WIDTH,D400_HEIGHT,D400_WIDTH,g[0].width,g[0].height,g[0].stride_pixels,
     ox,oy,129+D400_HALO,96+D400_HALO,cache0,cache1))goto done;
  memset((uint8_t*)map+g[0].content_bytes,0,actual[0]-g[0].content_bytes);
  if(duss_hal_mem_sync(raw,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(raw))goto done;map=0;
  jpeg_elapsed("DISK400_RAW_READ_MS ",begin,jpeg_monotonic_ms());
  if(duss_hal_mem_share(raw,handles)||duss_hal_mem_share(yuv,handles+1)||
     duss_hal_mem_get_phys_addr(raw,identities)||duss_hal_mem_get_phys_addr(yuv,identities+1)||
     !identities[0]||!identities[1]||identities[0]==identities[1])goto done;
  sizes[0]=actual[0];sizes[1]=actual[1];request=rebuild_request_sized(event,handles,g,sizes);
  if(!request||!dbus_message_set_sender(request,0)||
     !dbus_message_set_destination(request,"com.hasselblad.camera")||!factory_route_publish(identities))goto done;
  submitted=1;phase="RENDER";text("TILE_SUBMIT ");number(tile);text("\n");
  StackDBusError error;dbus_error_init(&error);
  begin=jpeg_monotonic_ms();
  reply=dbus_connection_send_with_reply_and_block(bus,request,20000,&error);
  jpeg_elapsed("DISK400_NATIVE_REQUEST_MS ",begin,jpeg_monotonic_ms());
  if(!reply){text("DISK400_NATIVE_NO_REPLY error=");text(stack_dbus_error_category(error.name));text("\n");}
  dbus_error_free(&error);
  if(!reply)goto done;
  phase="RENDER_REPLY";
  if(dbus_message_get_type(reply)!=2){text("DISK400_NATIVE_REPLY_ERROR error=");text(stack_dbus_error_category(dbus_message_get_error_name(reply)));text("\n");goto done;}
  phase="RENDER_ROUTE_ACK";
  if(!factory_route_acknowledged()){text("DISK400_NATIVE_BRANCH_ACK_MISSING\n");goto done;}
  phase="RENDER_REPLY_BODY";
  Iter result={0};if(dbus_message_iter_init(reply,&result)){text("DISK400_NATIVE_REPLY_BODY_UNEXPECTED\n");goto done;}
  phase="RENDER_DRAIN";if(!factory_stop_native(bus))goto done;
  phase="RENDER_ROUTE_CLEAR";if(!factory_route_clear())goto done;submitted=0;
  dbus_message_unref(reply);reply=0;dbus_message_unref(request);request=0;
  if(!disk400_release(&raw))goto done;
  text("TILE_COPIED ");number(tile);text("\n");
  phase="SHARPEN_AND_CROP";
  if(duss_hal_mem_map(yuv,&map)||!map||duss_hal_mem_sync(yuv,FACTORY_SYNC_CPU)||
     !factory_reference_sharpen((uint8_t*)map+g[1].planes[0].offset,g[1].width,g[1].height,g[1].stride_pixels))goto done;
  unsigned w=D400_WIDTH-ox,h=D400_HEIGHT-oy;
  if(w>D400_STEP_X)w=D400_STEP_X;if(h>D400_STEP_Y)h=D400_STEP_Y;
  /* 裁掉锐化外围。输出平面仍在同一块 YUV 内，向低地址逐行移动，
   * 色度按 NV16 的偶数 X 保持相位；不用第二块 YUV。 */
  for(unsigned plane=0;plane<2;plane++)for(unsigned row=0;row<h;row++){
   uint8_t *dst=(uint8_t*)map+((uint64_t)plane*h+row)*g[1].stride_pixels;
   uint8_t *src=(uint8_t*)map+g[1].planes[plane].offset+(uint64_t)(row+D400_HALO)*g[1].stride_pixels+D400_HALO;
   memmove(dst,src,w);memset(dst+w,0,g[1].stride_pixels-w);
  }
  if(duss_hal_mem_sync(yuv,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(yuv))goto done;map=0;
  phase="JPEG";if(!disk400_progress(2,tile+1,tile)||!disk400_tile_path(path,tile))goto done;
  rc=encode_jpeg_sized_to(ion,yuv,w,h,g[1].stride_pixels,path,D400_JPEG_CAPACITY);
  if(rc)goto done;
  rc=86;if(!disk400_release(&yuv))goto done;
  text("DISK400_JPEG_TILE_SAVED ");number(tile++);text("\n");
  if(!disk400_progress(3,tile,tile))goto done;
 }
 rc=tile==9?0:86;if(!rc)text("DISK400_NINE_TILES_DURABLE_NO_FULL_IMAGE_BUFFER\n");
done:
 if(rc){text("DISK400_FAILURE phase=");text(phase);text(" tile=");number(tile);text("\n");}
 if(submitted&&!factory_stop_native(bus)){
  rc=75;text("DISK400_NATIVE_DRAIN_PENDING_DO_NOT_FREE_ACTIVE_BUFFERS\n");
  while(!factory_wait_native_idle(bus,20)){long delay[2]={1,0};sc(101,(long)delay,0,0,0);}
 }
 if(!factory_route_clear())rc=75;
 if(map&&duss_hal_mem_unmap(raw?raw:yuv))rc=87;
 if(!disk400_release(&raw)||!disk400_release(&yuv))rc=87;
 if(reply)dbus_message_unref(reply);if(request)dbus_message_unref(request);
 if(fd>=0&&sc(57,fd,0,0,0))rc=87;
 return rc;
}
