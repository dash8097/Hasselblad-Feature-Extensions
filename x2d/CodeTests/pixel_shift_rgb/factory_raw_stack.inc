/* 4.2.0 RAW 堆栈：原尺寸单次显影；只读本任务最后一张合成 RAW。 */
#include "capture_runtime_job.h"
#include "factory_raw_stack_diagnostics.h"
#ifdef FACTORY_QUARTER_SINGLE
#include "factory_quarter_single.inc"
#endif
static int stack_path(char out[192],const char *job,const char *leaf){
 unsigned a=0,b=0;while(job[a]&&a<108){out[a]=job[a];a++;}if(a>=108)return 0;
 out[a++]='/';do{if(a+b>=192)return 0;out[a+b]=leaf[b];}while(leaf[b++]);return 1;
}
static int stack_read(int fd,void *data,uint32_t bytes,uint64_t offset){
 uint8_t *out=data;
 while(bytes){long n=jpeg_sc(67,fd,(long)out,bytes,(long)offset,0,0);
  if(n==-4)continue;if(n<=0||(unsigned long)n>bytes)return 0;out+=n;bytes-=(uint32_t)n;offset+=(uint64_t)n;}
 return 1;
}
#ifdef FACTORY_QUARTER_MEMORY
#include "factory_quarter_memory.inc"
static int quarter_handoff_check;
#endif
static int stack_preview(void *ion,void *yuv,const FactoryBufferGeometry *source){
 void *small=0,*input=0,*target=0;FactoryBufferGeometry g;int rc=79;
 if(!factory_buffer_geometry(&g,3888,2918,6)||
    duss_hal_mem_alloc(ion,&small,g.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||!small||
    duss_hal_mem_map(yuv,&input)||!input||duss_hal_mem_sync(yuv,FACTORY_SYNC_CPU)||
    duss_hal_mem_map(small,&target)||!target)goto done;
 /* NV16 亮度与 UV 成对采样；仅生成相册预览，完整成片不做缩放。 */
 for(unsigned y=0;y<g.height;y++){
  unsigned sy=(uint64_t)y*source->height/g.height;
  uint8_t *dy=(uint8_t*)target+(uint64_t)y*g.stride_pixels;
  uint8_t *duv=(uint8_t*)target+g.planes[1].offset+(uint64_t)y*g.stride_pixels;
  const uint8_t *py=(uint8_t*)input+(uint64_t)sy*source->stride_pixels;
  const uint8_t *puv=(uint8_t*)input+source->planes[1].offset+(uint64_t)sy*source->stride_pixels;
  for(unsigned x=0;x<g.width;x++)dy[x]=py[(uint64_t)x*source->width/g.width];
  for(unsigned x=0;x<g.width;x+=2){unsigned sx=((uint64_t)x*source->width/g.width)&~1u;duv[x]=puv[sx];duv[x+1]=puv[sx+1];}
  memset(dy+g.width,0,g.stride_pixels-g.width);memset(duv+g.width,0,g.stride_pixels-g.width);
 }
 if(duss_hal_mem_unmap(yuv))goto done;input=0;
 if(duss_hal_mem_sync(small,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(small))goto done;target=0;
 rc=encode_jpeg_sized_to(ion,small,g.width,g.height,g.stride_pixels,runtime_preview_output,16u*1024*1024);
done:
 if(input&&duss_hal_mem_unmap(yuv))rc=79;if(target&&duss_hal_mem_unmap(small))rc=79;
 if(small&&duss_hal_mem_free(small))rc=79;return rc;
}
static int run_raw_stack(const char *requested_job){
 const char *phase="job";
 char job[108],path[192];int rc=78,fd=-1,init=0,start=0,submitted=0,config=0;
 void *bus=0,*ion=0,*raw=0,*yuv=0,*map=0,*event=0,*request=0,*reply=0,*metadata=0;
 uint64_t metadata_bytes=0,handles[2]={0},identities[2]={0},sizes[2];uint32_t actual[2]={0};
 FactoryBufferGeometry geometry[2];uint8_t header[16384];
 struct module modules[]={{"/dev/ion",duss_hal_attach_ion_mem,duss_hal_detach_ion_mem,0},
  {"/dev/ienc0",duss_hal_attach_verislcon_ienc,duss_hal_detach_verislcon_ienc,0},{0,0,0,0}};
 if(!capture_runtime_job(job,requested_job))goto done;
#ifdef FACTORY_QUARTER_MEMORY
 if(!stack_path(path,job,"quarter.memory.ready"))goto done;
#else
 if(!stack_path(path,job,"raw.ready"))goto done;
#endif
 phase="ready";
 fd=(int)sc(56,-100,(long)path,0xA0000,0);if(fd<0)goto done;
 char ready[32];long n=sc(63,fd,(long)ready,sizeof ready,0);sc(57,fd,0,0,0);fd=-1;
#ifdef FACTORY_QUARTER_MEMORY
 if(n!=18||memcmp(ready,"QUARTER_MEMORY_R1\n",18))goto done;
#else
 if(n!=13||memcmp(ready,"RAW_STACK_R1\n",13))goto done;
#endif
#ifdef FACTORY_QUARTER_SINGLE
 /* 独立候选只消费确实已完成的一帧平均任务，不把多帧堆栈当单拍。 */
 phase="frame";
 if(!stack_path(path,job,"frame.done"))goto done;
 fd=(int)sc(56,-100,(long)path,0xA0000,0);if(fd<0)goto done;
 char single[7];n=sc(63,fd,(long)single,sizeof single,0);sc(57,fd,0,0,0);fd=-1;
 if(n!=6||memcmp(single,"RS1 1\n",6))goto done;
#endif
 phase="metadata";
 if(!stack_path(path,job,"rawpb.bin"))goto done;fd=(int)sc(56,-100,(long)path,0xA0000,0);if(fd<0)goto done;
 long length=sc(62,fd,0,2,0);if(length<=0||length>CBW_METADATA_MAX)goto done;metadata_bytes=(uint64_t)length;
 long address=jpeg_sc(222,0,length,3,0x22,-1,0);if(address<0&&address>=-4095)goto done;metadata=(void*)address;
 if(!stack_read(fd,metadata,(uint32_t)length,0)||!crm_rawpb_wb_valid(metadata,(size_t)length)||
    !(event=memory_request_template(metadata,(unsigned)length)))goto done;
 if(sc(57,fd,0,0,0))goto done;fd=-1;
 if(sc(215,(long)metadata,length,0,0))goto done;metadata=0;
#ifdef FACTORY_QUARTER_MEMORY
 if(!stack_path(path,job,"source.header"))goto done;
#else
 if(!stack_path(path,job,"merged.3FR"))goto done;
#endif
 fd=(int)sc(56,-100,(long)path,0xA0000,0);if(fd<0)goto done;
 if(!stack_read(fd,header,sizeof header,0))goto done;
 phase="source-header";
 uint32_t main=f3_u32(header+4),raw_ifd;
 if(memcmp(header,"II*\0",4)||!f3_scalar(header,sizeof header,main,330,&raw_ifd)||
    !f3_is(header,sizeof header,raw_ifd,256,11904)||!f3_is(header,sizeof header,raw_ifd,257,8842)||
    !f3_is(header,sizeof header,raw_ifd,273,16384)||!f3_is(header,sizeof header,raw_ifd,279,210510336))goto done;
 if(!stack_path(path,job,"render.jpg")||strlen(path)>=sizeof runtime_jpeg_output)goto done;strcpy(runtime_jpeg_output,path);
 if(!stack_path(path,job,"preview.jpg")||strlen(path)>=sizeof runtime_preview_output)goto done;strcpy(runtime_preview_output,path);
 phase="native-idle-and-hal";
 bus=dbus_bus_get(1,0);if(!bus)goto done;
#ifndef FACTORY_QUARTER_MEMORY
 if(factory_native_group(bus)!=255)goto done;
#endif
 /* 共享段交接无需显影空闲；恢复放行之后才检查原厂显影通道。 */
 if(duss_hal_initialize(modules))goto done;init=1;
 if(duss_hal_device_open("/dev/ion",&config,&ion)||duss_hal_device_start(ion,&config))goto done;start=1;
 phase="raw-allocation";
 if(!factory_buffer_geometry(&geometry[0],11836,8842,1)||!factory_buffer_geometry(&geometry[1],11656,8742,6)||
    duss_hal_mem_alloc(ion,&raw,geometry[0].allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
#ifndef FACTORY_QUARTER_MEMORY
    duss_hal_mem_alloc(ion,&yuv,geometry[1].allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
    duss_hal_mem_get_size(yuv,&actual[1])||!factory_buffer_fits(&geometry[1],actual[1])||
#endif
    duss_hal_mem_get_size(raw,&actual[0])||!factory_buffer_fits(&geometry[0],actual[0])||
    duss_hal_mem_map(raw,&map)||!map)goto done;
#ifdef FACTORY_QUARTER_MEMORY
 phase="shared-copy";
 if(quarter_handoff_check){rc=0;goto done;}
 if(!quarter_copy_shared(ion,raw,map,job))goto done;
#else
 if(!stack_read(fd,map,geometry[0].content_bytes,16384))goto done;
#endif
 memset((uint8_t*)map+geometry[0].content_bytes,0,actual[0]-geometry[0].content_bytes);
 if(duss_hal_mem_sync(raw,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(raw))goto done;map=0;
 if(sc(57,fd,0,0,0))goto done;fd=-1;
#ifdef FACTORY_QUARTER_MEMORY
 phase="render-allow-and-yuv";
 if(!quarter_allowed(job)||factory_native_group(bus)!=255||
    duss_hal_mem_alloc(ion,&yuv,geometry[1].allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
    duss_hal_mem_get_size(yuv,&actual[1])||!factory_buffer_fits(&geometry[1],actual[1]))goto done;
#endif
 phase="native-share";
 if(duss_hal_mem_share(raw,&handles[0])||duss_hal_mem_share(yuv,&handles[1])||
    duss_hal_mem_get_phys_addr(raw,&identities[0])||duss_hal_mem_get_phys_addr(yuv,&identities[1]))goto done;
 phase="native-request-build";
 sizes[0]=actual[0];sizes[1]=actual[1];request=rebuild_request_sized(event,handles,geometry,sizes);
 if(!request)goto done;
 phase="native-route-publish";
 if(!factory_route_publish(identities))goto done;submitted=1;
 phase="native-submit";
 text("RAW_STACK_NATIVE_SINGLE_WINDOW_SUBMIT\n");
 StackDBusError error;dbus_error_init(&error);uint64_t submit_begin=jpeg_monotonic_ms();
 reply=dbus_connection_send_with_reply_and_block(bus,request,20000,&error);
 jpeg_elapsed("RAW_STACK_NATIVE_REQUEST_MS ",submit_begin,jpeg_monotonic_ms());
 if(!reply){text("RAW_STACK_NATIVE_NO_REPLY error=");text(stack_dbus_error_category(error.name));text("\n");}
 dbus_error_free(&error);
 if(!reply)goto done;
 phase="native-reply-type";
 int reply_type=dbus_message_get_type(reply);
 if(reply_type!=2){
  stack_diagnostic_status("reply-type",reply_type);
  if(reply_type==3){text("RAW_STACK_NATIVE_REPLY_ERROR error=");text(stack_dbus_error_category(dbus_message_get_error_name(reply)));text("\n");}
  goto done;
 }
 phase="native-route-ack";
 if(!factory_route_acknowledged()){text("RAW_STACK_NATIVE_BRANCH_ACK_MISSING\n");goto done;}
 phase="native-reply-body";
 Iter result={0};if(dbus_message_iter_init(reply,&result)){text("RAW_STACK_NATIVE_REPLY_BODY_UNEXPECTED\n");goto done;}
 phase="native-stop";
 if(!factory_stop_native(bus)){text("RAW_STACK_NATIVE_STOP_FAILED\n");goto done;}
 phase="native-route-clear";
 if(!factory_route_clear()){text("RAW_STACK_NATIVE_ROUTE_CLEAR_FAILED\n");goto done;}submitted=0;
 /* 原厂不再消费 RAW 才释放，JPEG 输出缓冲只和最终 YUV 共存。 */
 phase="native-raw-release";
 int release_rc=duss_hal_mem_free(raw);stack_diagnostic_status("raw-release",release_rc);
 if(release_rc)goto done;raw=0;
#ifdef FACTORY_QUARTER_SINGLE
 phase="quarter-render";
 rc=quarter_render_small(ion,&yuv,&geometry[1]);if(rc)goto done;
#endif
 /* 一亿堆栈沿用原厂显影输出；不叠加四亿成片的额外亮度锐化。 */
 phase="preview";rc=stack_preview(ion,yuv,&geometry[1]);if(rc)goto done;
 phase="jpeg";
#ifdef FACTORY_QUARTER_SINGLE
 rc=encode_jpeg_sized_to(ion,yuv,geometry[1].width,geometry[1].height,geometry[1].stride_pixels,runtime_jpeg_output,64u*1024*1024);
 if(!rc&&!quarter_jpeg_visible_height(runtime_jpeg_output))rc=79;
#else
 rc=encode_jpeg_sized_to(ion,yuv,11656,8742,geometry[1].stride_pixels,runtime_jpeg_output,64u*1024*1024);
#endif
 if(!rc)text("RAW_STACK_NATIVE_JPEG_AND_FINAL_PREVIEW_COMPLETE\n");
done:
 /* 保留进入清理前的结果；原有返回码和缓冲释放顺序保持不变。 */
 ;int primary_rc=rc,cleanup_rc=0,status=0;uint64_t cleanup_begin=jpeg_monotonic_ms();
 text("RAW_STACK_RENDER_PHASE ");text(phase);text(" rc=");number((unsigned)rc);text("\n");
 text("RAW_STACK_RENDER_CLEANUP_BEGIN\n");
 if(submitted&&bus){status=factory_stop_native(bus);stack_diagnostic_status("cleanup-native-stop",status?0:75);
  if(!status){rc=cleanup_rc=75;text("RAW_STACK_RENDER_CLEANUP_WAIT_NATIVE_IDLE\n");
   while(!factory_wait_native_idle(bus,20)){long delay[2]={1,0};sc(101,(long)delay,0,0,0);}
   text("RAW_STACK_RENDER_CLEANUP_NATIVE_IDLE_CONFIRMED\n");}}
 if(bus){status=factory_route_clear();stack_diagnostic_status("cleanup-route-clear",status?0:75);if(!status)rc=cleanup_rc=75;}
 if(map){status=duss_hal_mem_unmap(raw);stack_diagnostic_status("cleanup-raw-unmap",status);if(status)rc=cleanup_rc=79;}
 if(raw){status=duss_hal_mem_free(raw);stack_diagnostic_status("cleanup-raw-free",status);if(status)rc=cleanup_rc=79;}
 if(yuv){status=duss_hal_mem_free(yuv);stack_diagnostic_status("cleanup-yuv-free",status);if(status)rc=cleanup_rc=79;}
 if(start){status=duss_hal_device_stop(ion,0);stack_diagnostic_status("cleanup-ion-stop",status);if(status)rc=cleanup_rc=79;}
 if(ion){status=duss_hal_device_close(ion);stack_diagnostic_status("cleanup-ion-close",status);if(status)rc=cleanup_rc=79;}
 if(init){status=duss_hal_deinitialize();stack_diagnostic_status("cleanup-hal-deinitialize",status);if(status)rc=cleanup_rc=79;}
 /* 这两项原先不改变进程退出码，诊断也不改变该行为。 */
 if(fd>=0){status=(int)sc(57,fd,0,0,0);stack_diagnostic_status("cleanup-file-close",status);if(status)cleanup_rc=79;}
 if(metadata){status=(int)sc(215,(long)metadata,metadata_bytes,0,0);stack_diagnostic_status("cleanup-metadata-unmap",status);if(status)cleanup_rc=79;}
 if(reply)dbus_message_unref(reply);if(request)dbus_message_unref(request);if(event)dbus_message_unref(event);
 if(bus)dbus_connection_unref(bus);
 jpeg_elapsed("RAW_STACK_RENDER_CLEANUP_MS ",cleanup_begin,jpeg_monotonic_ms());
 text("RAW_STACK_RENDER_EXIT primary_rc=");number((unsigned)primary_rc);
 text(" final_rc=");number((unsigned)rc);text(" cleanup_rc=");number((unsigned)cleanup_rc);text("\n");return rc;
}
