/* 独立候选：原厂原尺寸无HISO显影，再复制YUV平面并由原厂编码器输出四亿。
 * 不把分块拼接称为原厂整幅四亿协议支持；标定与接缝尚未验证。 */
#include "factory_raw_tile.h"
#ifdef FACTORY_PACKED_DNG_INPUT
#include "factory_packed_dng.h"
#endif
#ifdef FACTORY_SIX_PHASE_ROI
#include "factory_tile_copy.h"
#endif
#ifdef FACTORY_SIX_EDGE_TILES
#include "factory_six_tile_plan.h"
#endif
#if defined(FACTORY_NATIVE_PREVIEW_OUTPUT) && !defined(FACTORY_CAPTURE_PREVIEW)
#include "factory_preview_scale.h"
static int render_default_preview(void *ion,const void *source,const FactoryBufferGeometry *geometry){
 void *buffer=0,*pixels=0;uint32_t bytes=0;int rc=69;
 FactoryBufferGeometry preview;
 if(!factory_buffer_geometry(&preview,3888,2918,6)||
    duss_hal_mem_alloc(ion,&buffer,preview.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||!buffer)goto end;
 if(duss_hal_mem_get_size(buffer,&bytes)||!factory_buffer_fits(&preview,bytes)||
    duss_hal_mem_map(buffer,&pixels)||!pixels)goto end;
 uint64_t scale_begin=jpeg_monotonic_ms();
 if(!factory_preview_scale(source,geometry->width,geometry->height,geometry->stride_pixels,
    pixels,preview.width,preview.height,preview.stride_pixels))goto end;
 jpeg_elapsed("PREVIEW_DOWNSAMPLE_MS ",scale_begin,jpeg_monotonic_ms());
 for(uint32_t i=preview.content_bytes;i<bytes;i++)((uint8_t*)pixels)[i]=0;
 if(duss_hal_mem_sync(buffer,FACTORY_SYNC_DEVICE))goto end;
 if(duss_hal_mem_unmap(buffer))goto end;pixels=0;
 rc=encode_jpeg_sized_to(ion,buffer,preview.width,preview.height,preview.stride_pixels,
    FACTORY_NATIVE_PREVIEW_OUTPUT,32*1024*1024u);
 if(!rc)text("DEFAULT_PREVIEW_GENERATED_FROM_MERGED_YUV\n");
end:
 if(pixels&&duss_hal_mem_unmap(buffer))rc=69;
 if(buffer&&duss_hal_mem_free(buffer))rc=69;
 return rc;
}
#endif
#ifdef FACTORY_JPEG_LIGHT_SHARPEN
#include "factory_light_sharpen.h"
static uint8_t sharpen_scratch[3*23326];
#endif
typedef struct {int fd;uint32_t base;} TileInput;
static int tile_read_at(void*context,uint64_t offset,unsigned char*dest,uint32_t size){
 TileInput *input=context;int fd=input->fd;offset+=input->base;
 if(sc(62,fd,(long)offset,0,0)!=(long)offset)return 0;
 uint32_t done=0;
 while(done<size){long n=candidate_read(&fd,dest+done,size-done);if(n<=0||(uint64_t)n>size-done)return 0;done+=(uint32_t)n;}
 return 1;
}
static int render_tiled_408(void*bus,void*event,void*ion){
 void *raw=0,*tile=0,*full=0,*raw_map=0,*tile_map=0,*full_map=0,*request=0,*reply=0;
 uint64_t shared[2]={0};uint32_t raw_size=0,tile_size=0,full_size=0;
 int fd=-1,rc=60;FactoryBufferGeometry graw,gtile,gfull;
 uint16_t cache0[23326],cache1[23326];
 if(!factory_buffer_geometry(&graw,11836,8842,1)||!factory_buffer_geometry(&gtile,11656,8742,6)||
    !factory_buffer_geometry(&gfull,23326,17498,6))goto done;
 fd=(int)sc(56,-100,(long)candidate_raw_path,0x20000,0);if(fd<0)goto done;
 TileInput input={fd,0};uint32_t source_stride=23424;
#ifdef FACTORY_PACKED_DNG_INPUT
 uint8_t packed_header[4096];uint64_t source_bytes=(uint64_t)sc(62,fd,0,2,0);
 if(!tile_read_at(&input,0,packed_header,4096)||
    !factory_packed_dng_geometry(packed_header,source_bytes,1,&source_stride,&input.base))goto done;
 text("PACKED_MERGED_RAW_READ_DIRECTLY_NO_LAYOUT_COPY\n");
#else
 if(sc(62,fd,0,2,0)!=819746304)goto done;
#endif
 text("TILED_NATIVE_ALLOCATE_BEGIN\n");sc(82,1,0,0,0);
 uint64_t allocate_begin=jpeg_monotonic_ms();
 rc=61;
 if(duss_hal_mem_alloc(ion,&raw,graw.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
    duss_hal_mem_alloc(ion,&tile,gtile.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
    duss_hal_mem_alloc(ion,&full,gfull.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS))goto done;
 if(duss_hal_mem_get_size(raw,&raw_size)||duss_hal_mem_get_size(tile,&tile_size)||duss_hal_mem_get_size(full,&full_size))goto done;
 if(!factory_buffer_fits(&graw,raw_size)||!factory_buffer_fits(&gtile,tile_size)||!factory_buffer_fits(&gfull,full_size))goto done;
 text("TILED_NATIVE_ALLOCATE_OK\n");sc(82,1,0,0,0);
 jpeg_elapsed("TILE_ALLOCATION_MS ",allocate_begin,jpeg_monotonic_ms());
 if(duss_hal_mem_share(raw,&shared[0])||duss_hal_mem_share(tile,&shared[1]))goto done;
#ifdef FACTORY_INPROCESS_ROUTE
 uint64_t identities[2]={0};
 if(duss_hal_mem_get_phys_addr(raw,&identities[0])||duss_hal_mem_get_phys_addr(tile,&identities[1])||
    !identities[0]||!identities[1]||identities[0]==identities[1])goto done;
#endif
#ifndef FACTORY_SIX_EDGE_TILES
 request=rebuild_request(event,shared);if(!request)goto done;
 if(!dbus_message_set_sender(request,0)||!dbus_message_set_destination(request,"com.hasselblad.camera"))goto done;
#endif
 rc=62;if(duss_hal_mem_map(full,&full_map)||!full_map)goto done;
 /* 全幅行尾留零；有效像素在各分块成功后填入。 */
 for(unsigned p=0;p<2;p++)for(uint32_t y=0;y<gfull.height;y++)
  for(uint32_t x=gfull.width;x<gfull.stride_pixels;x++)
   ((unsigned char*)full_map)[gfull.planes[p].offset+(uint64_t)y*gfull.stride_pixels+x]=0;
 unsigned index=0;
 uint32_t step_x=gtile.width,crop_x=128;
#ifdef FACTORY_SIX_PHASE_ROI
 /* 保留两列余量，使丢弃左侧重叠列后的宽度仍为偶数。 */
 step_x-=2;crop_x++;
#endif
#ifdef FACTORY_SIX_EDGE_TILES
 for(unsigned planned=0;planned<6;planned++){
   FactorySixTile piece;if(!factory_six_tile(planned,&piece))goto done;
   uint32_t ox=piece.x,oy=piece.y;
   graw=piece.raw;gtile=piece.yuv;step_x=piece.width;
   FactoryBufferGeometry selected[2]={graw,gtile};uint64_t allocated[2]={raw_size,tile_size};
   if(request){dbus_message_unref(request);request=0;}
   request=rebuild_request_sized(event,shared,selected,allocated);if(!request)goto done;
   if(!dbus_message_set_sender(request,0)||!dbus_message_set_destination(request,"com.hasselblad.camera"))goto done;
   if(gtile.allocation_bytes<tile_size){
    if(duss_hal_mem_map(tile,&tile_map)||!tile_map)goto done;
    for(uint32_t at=gtile.allocation_bytes;at<tile_size;at++)((uint8_t*)tile_map)[at]=0xa5;
    if(duss_hal_mem_sync(tile,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(tile))goto done;tile_map=0;
   }
#else
 for(uint32_t oy=0;oy<gfull.height;oy+=gtile.height){
  for(uint32_t ox=0;ox<gfull.width;ox+=step_x){
#endif
   text("TILE_TIMING_INDEX ");number(index);text("\n");
   uint64_t raw_begin=jpeg_monotonic_ms();
   rc=63;if(duss_hal_mem_map(raw,&raw_map)||!raw_map)goto done;
   if(!factory_raw_tile(tile_read_at,&input,raw_map,23326,17498,source_stride,graw.width,graw.height,graw.stride_pixels,
      ox,oy,crop_x,96,cache0,cache1))goto done;
   for(uint32_t j=graw.content_bytes;j<raw_size;j++)((unsigned char*)raw_map)[j]=0;
   if(duss_hal_mem_sync(raw,FACTORY_SYNC_DEVICE))goto done;
   if(duss_hal_mem_unmap(raw))goto done;raw_map=0;
   jpeg_elapsed("TILE_RAW_PREPARE_MS ",raw_begin,jpeg_monotonic_ms());
   text("TILE_SUBMIT ");number(index);text("\n");sc(82,1,0,0,0);
   /* 每次复制新的消息，避免重用已发送消息的序号/锁定状态。 */
   void*send=dbus_message_copy(request);rc=64;if(!send)goto done;
#ifdef FACTORY_INPROCESS_ROUTE
   if(!factory_route_publish(identities)){dbus_message_unref(send);goto done;}
#endif
   uint64_t request_begin=jpeg_monotonic_ms();
   reply=dbus_connection_send_with_reply_and_block(bus,send,20000,0);dbus_message_unref(send);
   jpeg_elapsed("TILE_NATIVE_REQUEST_MS ",request_begin,jpeg_monotonic_ms());
   if(!reply){text("TILE_NO_REPLY\n");goto done;}
   if(dbus_message_get_type(reply)!=2){
    text("TILE_REPLY_ERROR ");const char *name=dbus_message_get_error_name(reply);
    if(name)text(name);text("\n");goto done;
   }
#ifdef FACTORY_INPROCESS_ROUTE
   if(!factory_route_acknowledged()){text("TILE_BRANCH_ACK_MISSING\n");goto done;}
   text("TILE_TASK_BRANCH_CONFIRMED ");number(index);text("\n");
#endif
   Iter result={0};if(dbus_message_iter_init(reply,&result))goto done;
   dbus_message_unref(reply);reply=0;
   uint64_t copy_begin=jpeg_monotonic_ms();
   rc=65;if(duss_hal_mem_map(tile,&tile_map)||!tile_map||duss_hal_mem_sync(tile,FACTORY_SYNC_CPU))goto done;
#ifdef FACTORY_SIX_EDGE_TILES
   for(uint32_t at=gtile.allocation_bytes;at<tile_size;at++)if(((uint8_t*)tile_map)[at]!=0xa5){
    text("NARROW_TILE_CAPACITY_GUARD_CHANGED\n");goto done;
   }
#endif
   uint32_t width=gfull.width-ox,height=gfull.height-oy;
   if(width>step_x)width=step_x;if(height>gtile.height)height=gtile.height;
#ifdef FACTORY_SIX_PHASE_ROI
   if(!factory_tile_copy((const uint8_t*)tile_map+gtile.planes[0].offset,
      (const uint8_t*)tile_map+gtile.planes[1].offset,
      gtile.width,gtile.height,gtile.stride_pixels,1,
      (uint8_t*)full_map+gfull.planes[0].offset,(uint8_t*)full_map+gfull.planes[1].offset,
      gfull.width,gfull.height,gfull.stride_pixels,ox,oy,width,height))goto done;
#else
   for(unsigned p=0;p<2;p++)for(uint32_t y=0;y<height;y++){
    unsigned char*dst=(unsigned char*)full_map+gfull.planes[p].offset+(uint64_t)(oy+y)*gfull.stride_pixels+ox;
    const unsigned char*src=(unsigned char*)tile_map+gtile.planes[p].offset+(uint64_t)y*gtile.stride_pixels;
    for(uint32_t x=0;x<width;x++)dst[x]=src[x];
   }
#endif
   /* 只读取 tile，没有 CPU 修改，不再次刷回设备。 */
   if(duss_hal_mem_unmap(tile))goto done;tile_map=0;
   jpeg_elapsed("TILE_ASSEMBLE_MS ",copy_begin,jpeg_monotonic_ms());
   text("TILE_COPIED ");number(index++);text("\n");sc(82,1,0,0,0);
#ifndef FACTORY_SIX_EDGE_TILES
  }
#endif
 }
 rc=66;
#ifdef FACTORY_SIX_EDGE_TILES
 if(index!=6)goto done;
#else
 if(index!=9)goto done;
#endif
 /* 分块已结束，先释放两块工作区，为默认预览留出分配空间。 */
 if(duss_hal_mem_free(raw))goto done;raw=0;
 if(duss_hal_mem_free(tile))goto done;tile=0;
#ifdef FACTORY_JPEG_LIGHT_SHARPEN
 if(!factory_light_sharpen((uint8_t*)full_map+gfull.planes[0].offset,
    gfull.width,gfull.height,gfull.stride_pixels,sharpen_scratch,sizeof sharpen_scratch))goto done;
 text("LIGHT_LUMA_SHARPEN_APPLIED\n");
#endif
#if defined(FACTORY_NATIVE_PREVIEW_OUTPUT) && !defined(FACTORY_CAPTURE_PREVIEW)
 rc=render_default_preview(ion,full_map,&gfull);if(rc)goto done;
#endif
#ifdef FACTORY_CAPTURE_PREVIEW
 text("DEFAULT_PREVIEW_REUSED_FROM_CAPTURE\n");
#endif
 if(duss_hal_mem_sync(full,FACTORY_SYNC_DEVICE))goto done;
 if(duss_hal_mem_unmap(full))goto done;full_map=0;
#ifdef FACTORY_SIX_EDGE_TILES
 text("SIX_NATIVE_TILES_ASSEMBLED_CALIBRATION_UNVERIFIED\n");
#else
 text("NINE_NATIVE_TILES_ASSEMBLED_CALIBRATION_UNVERIFIED\n");
#endif
 sc(82,1,0,0,0);
 rc=encode_jpeg_sized(ion,full,23326,17498,23552);
done:
 if(reply)dbus_message_unref(reply);if(request)dbus_message_unref(request);
 if(fd>=0)sc(57,fd,0,0,0);
 if(raw_map&&duss_hal_mem_unmap(raw))rc=67;
 if(tile_map&&duss_hal_mem_unmap(tile))rc=67;
 if(full_map&&duss_hal_mem_unmap(full))rc=67;
 if(full&&duss_hal_mem_free(full))rc=68;if(tile&&duss_hal_mem_free(tile))rc=68;if(raw&&duss_hal_mem_free(raw))rc=68;
 return rc;
}
