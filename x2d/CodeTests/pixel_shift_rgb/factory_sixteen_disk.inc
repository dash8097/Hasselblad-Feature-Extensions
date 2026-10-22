/* 十六张磁盘合成候选。复用现行 RAW 堆栈的 native metadata 请求和路由。
 * 一块 RAW -> 原厂 YUV -> 原厂 JPEG -> SSD；不分配整幅 YUV。
 * 所有临时块固定留在本次 multishot 子目录；本程序不发布或删除原片。 */
#include "sixteen_disk_policy.h"
#include "sixteen_progress.h"
#ifndef FACTORY_SIXTEEN_HOST_CHECK
#include "factory_raw_tile.h"
#endif
typedef struct {int fd;} SixteenImageSource;
static int sixteen_progress_write(const char *job,unsigned phase,unsigned rendered,unsigned encoded){
 char tmp[192],path[192],line[32];unsigned n=sixteen_progress_line(line,phase,rendered,encoded);
 if(!n||!stack_path(tmp,job,"sixteen.progress.tmp")||!stack_path(path,job,"sixteen.progress"))return 0;
 int fd=(int)sc(56,-100,(long)tmp,0xA0241,0600);if(fd<0)return 0;
 int ok=sc(64,fd,(long)line,n,0)==n;
 if(sc(57,fd,0,0,0))ok=0;
 return ok&&!sc(38,-100,(long)tmp,-100,(long)path);
}
static int sixteen_image_read(void *context,uint64_t offset,unsigned char *out,uint32_t bytes){
 SixteenImageSource *image=context;
 const uint64_t length=(uint64_t)X2D_SIXTEEN_OUTPUT_WIDTH*X2D_SIXTEEN_OUTPUT_HEIGHT*2;
 return image&&out&&offset<=length&&bytes<=length-offset&&
  stack_read(image->fd,out,bytes,16384+offset);
}
static int sixteen_tile_path(char out[192],const char *job,unsigned index){
 char leaf[32]="render-tile-",digits[12];unsigned n=12,k=0;
 do{digits[k++]=(char)('0'+index%10);index/=10;}while(index);
 while(k)leaf[n++]=digits[--k];
 const char *suffix=".jpg";for(unsigned i=0;i<=4;i++)leaf[n+i]=suffix[i];
 return stack_path(out,job,leaf);
}
static int run_sixteen_disk(const char *requested_job){
 char job[108],path[192];int rc=86,fd=-1,init=0,start=0,submitted=0,config=0;
 void *bus=0,*ion=0,*raw=0,*yuv=0,*map=0,*event=0,*request=0,*reply=0,*metadata=0;
 uint64_t metadata_bytes=0,handles[2]={0},identities[2]={0},sizes[2];uint32_t actual[2]={0};
 FactoryBufferGeometry geometry[2];uint8_t header[16384];uint16_t cache0[11836],cache1[11836];
 const char *phase="INPUT";unsigned tile=0;
 struct module modules[]={{"/dev/ion",duss_hal_attach_ion_mem,duss_hal_detach_ion_mem,0},
  {"/dev/ienc0",duss_hal_attach_verislcon_ienc,duss_hal_detach_verislcon_ienc,0},{0,0,0,0}};
 if(!capture_runtime_job(job,requested_job)||!stack_path(path,job,"rawpb.bin"))goto done;
 fd=(int)sc(56,-100,(long)path,0xA0000,0);if(fd<0)goto done;
 long length=sc(62,fd,0,2,0);if(length<=0||length>CBW_METADATA_MAX)goto done;
 metadata_bytes=(uint64_t)length;long address=jpeg_sc(222,0,length,3,0x22,-1,0);
 if(address<0&&address>=-4095)goto done;metadata=(void*)address;
 if(!stack_read(fd,metadata,(uint32_t)length,0)||!crm_rawpb_wb_valid(metadata,(size_t)length)||
    !(event=memory_request_template(metadata,(unsigned)length)))goto done;
 if(sc(57,fd,0,0,0))goto done;fd=-1;
 if(sc(215,(long)metadata,length,0,0))goto done;metadata=0;
 if(!stack_path(path,job,"merged.3FR"))goto done;
 fd=(int)sc(56,-100,(long)path,0xA0000,0);if(fd<0||!stack_read(fd,header,sizeof header,0))goto done;
 uint32_t main=f3_u32(header+4),raw_ifd;
 if(memcmp(header,"II*\0",4)||!f3_scalar(header,sizeof header,main,330,&raw_ifd)||
    !f3_is(header,sizeof header,raw_ifd,256,X2D_SIXTEEN_OUTPUT_WIDTH)||
    !f3_is(header,sizeof header,raw_ifd,257,X2D_SIXTEEN_OUTPUT_HEIGHT)||
    !f3_is(header,sizeof header,raw_ifd,258,16)||!f3_is(header,sizeof header,raw_ifd,259,1)||
    !f3_is(header,sizeof header,raw_ifd,273,16384)||
    !f3_is(header,sizeof header,raw_ifd,279,UINT32_C(3265266784)))goto done;
 uint8_t *cfa=f3_data(header,sizeof header,f3_tag(header,sizeof header,raw_ifd,33422),1,4,1);
 const uint8_t rggb[4]={0,1,1,2};
 long file_bytes=sc(62,fd,0,2,0);
 if(!cfa||memcmp(cfa,rggb,4)||file_bytes<INT64_C(3265283168)||((uint64_t)file_bytes&4095))goto done;
 /* 禁止覆盖既有块。编码失败只删除新建的未完成块，已完成块留作诊断。 */
 for(unsigned i=0;i<25;i++){
  if(!sixteen_tile_path(path,job,i))goto done;
  long exists=sc(56,-100,(long)path,0xA0000,0);
  if(exists>=0){sc(57,exists,0,0,0);goto done;}if(exists!=-2)goto done;
 }
 phase="NATIVE_START";bus=dbus_bus_get(1,0);
 if(!bus||factory_native_group(bus)!=255||duss_hal_initialize(modules))goto done;init=1;
 if(duss_hal_device_open("/dev/ion",&config,&ion)||duss_hal_device_start(ion,&config))goto done;start=1;
 if(!factory_buffer_geometry(&geometry[0],11836,8842,1)||
    !factory_buffer_geometry(&geometry[1],11656,8742,6))goto done;
 SixteenImageSource image={fd};
 for(unsigned oy=0;oy<X2D_SIXTEEN_OUTPUT_HEIGHT;oy+=8736){
  for(unsigned ox=0;ox<X2D_SIXTEEN_OUTPUT_WIDTH;ox+=11648){
   if(!sixteen_progress_write(job,1,tile,tile))goto done;
   phase="ALLOCATE";rc=86;
   if(duss_hal_mem_alloc(ion,&raw,geometry[0].allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
      duss_hal_mem_alloc(ion,&yuv,geometry[1].allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
      !raw||!yuv||duss_hal_mem_get_size(raw,&actual[0])||duss_hal_mem_get_size(yuv,&actual[1])||
      !factory_buffer_fits(&geometry[0],actual[0])||!factory_buffer_fits(&geometry[1],actual[1]))goto done;
   phase="RAW_READ";uint64_t begin=jpeg_monotonic_ms();
   if(duss_hal_mem_map(raw,&map)||!map||
      !factory_raw_tile(sixteen_image_read,&image,map,X2D_SIXTEEN_OUTPUT_WIDTH,X2D_SIXTEEN_OUTPUT_HEIGHT,
       X2D_SIXTEEN_OUTPUT_WIDTH,geometry[0].width,geometry[0].height,geometry[0].stride_pixels,
       ox,oy,128,96,cache0,cache1))goto done;
   memset((uint8_t*)map+geometry[0].content_bytes,0,actual[0]-geometry[0].content_bytes);
   if(duss_hal_mem_sync(raw,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(raw))goto done;map=0;
   jpeg_elapsed("SIXTEEN_TILE_RAW_READ_MS ",begin,jpeg_monotonic_ms());
   if(duss_hal_mem_share(raw,&handles[0])||duss_hal_mem_share(yuv,&handles[1])||
      duss_hal_mem_get_phys_addr(raw,&identities[0])||duss_hal_mem_get_phys_addr(yuv,&identities[1])||
      !identities[0]||!identities[1]||identities[0]==identities[1])goto done;
   sizes[0]=actual[0];sizes[1]=actual[1];request=rebuild_request_sized(event,handles,geometry,sizes);
   if(!request||!dbus_message_set_sender(request,0)||
      !dbus_message_set_destination(request,"com.hasselblad.camera")||!factory_route_publish(identities))goto done;
   submitted=1;phase="NATIVE_RENDER";begin=jpeg_monotonic_ms();
   text("SIXTEEN_TILE_SUBMIT ");number(tile);text("\n");
   reply=dbus_connection_send_with_reply_and_block(bus,request,20000,0);
   jpeg_elapsed("SIXTEEN_TILE_NATIVE_RENDER_MS ",begin,jpeg_monotonic_ms());
   if(!reply){text("SIXTEEN_NATIVE_REPLY_MISSING\n");goto done;}
   if(dbus_message_get_type(reply)!=2){text("SIXTEEN_NATIVE_REPLY_REJECTED\n");goto done;}
   if(!factory_route_acknowledged()){text("SIXTEEN_NATIVE_ROUTE_ACK_MISSING\n");goto done;}
   Iter result={0};if(dbus_message_iter_init(reply,&result)){
    text("SIXTEEN_NATIVE_REPLY_UNEXPECTED_BODY\n");goto done;
   }
   if(!factory_stop_native(bus)||!factory_route_clear())goto done;submitted=0;
   dbus_message_unref(reply);reply=0;dbus_message_unref(request);request=0;
   /* RAW 已无人消费才释放。512 MiB 编码输出只和当前 YUV 共存。 */
   if(duss_hal_mem_free(raw))goto done;raw=0;
   #ifdef FACTORY_JPEG_REFERENCE_SHARPEN
   phase="JPEG_SHARPEN";uint64_t sharpen_begin=jpeg_monotonic_ms();
   if(duss_hal_mem_map(yuv,&map)||!map||
      !factory_reference_sharpen((uint8_t*)map+geometry[1].planes[0].offset,
       geometry[1].width,geometry[1].height,geometry[1].stride_pixels))goto done;
   if(duss_hal_mem_sync(yuv,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(yuv))goto done;map=0;
   jpeg_elapsed("SIXTEEN_TILE_SHARPEN_MS ",sharpen_begin,jpeg_monotonic_ms());
   text("SIXTEEN_TILE_SHARPEN_APPLIED amount=");number(FS_AMOUNT);
   text(" radius_tenths=");number(FS_RADIUS_TENTHS);text(" threshold=1 halo=0 threads=4\n");
   #endif
   phase="NATIVE_JPEG";
   if(!sixteen_progress_write(job,2,tile+1,tile))goto done;
   if(!sixteen_tile_path(path,job,tile))goto done;
   rc=encode_jpeg_sized_to(ion,yuv,geometry[1].width,geometry[1].height,
      geometry[1].stride_pixels,path,X2D_SIXTEEN_JPEG_CAPACITY);
   if(rc)goto done;
   if(duss_hal_mem_free(yuv)){rc=87;goto done;}yuv=0;
   text("SIXTEEN_TILE_ENCODED_TO_MULTISHOT ");number(tile++);text("\n");
   if(!sixteen_progress_write(job,3,tile,tile)){rc=86;goto done;}
  }
 }
 rc=tile==25?0:86;if(!rc)text("SIXTEEN_25_NATIVE_TILES_COMPLETE_NO_FULL_YUV\n");
done:
 if(submitted&&bus){if(!factory_stop_native(bus)){rc=75;
  while(!factory_wait_native_idle(bus,20)){long delay[2]={1,0};sc(101,(long)delay,0,0,0);}}}
 if(bus&&!factory_route_clear())rc=75;
 if(map&&duss_hal_mem_unmap(raw?raw:yuv))rc=87;
 if(raw&&duss_hal_mem_free(raw))rc=87;if(yuv&&duss_hal_mem_free(yuv))rc=87;
 if(start&&duss_hal_device_stop(ion,0))rc=87;if(ion&&duss_hal_device_close(ion))rc=87;
 if(init&&duss_hal_deinitialize())rc=87;
 if(fd>=0&&sc(57,fd,0,0,0))rc=87;
 if(metadata&&sc(215,(long)metadata,metadata_bytes,0,0))rc=87;
 if(reply)dbus_message_unref(reply);if(request)dbus_message_unref(request);if(event)dbus_message_unref(event);
 if(bus)dbus_connection_unref(bus);
 if(rc){text("SIXTEEN_RENDER_FAILURE phase=");text(phase);text(" tile=");number(tile);text("\n");}
 return rc;
}
