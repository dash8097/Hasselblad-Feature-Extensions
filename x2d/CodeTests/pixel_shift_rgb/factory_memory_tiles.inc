/* 4.2.0 固定 RAW/HISO 窗口。串行兼容分支在 FINISH 后读取成片；
 * 整幅分支在 FULL_READY（全部计算、六帧实际释放）后直接读共享结果，
 * 与必要 RAW 后台写入并行。FINISH 回复均等待显影、JPEG和缓冲释放。 */
#include "factory_native_3fr.h"
#include "memory_native_tile.h"
#include "capture_rawpb_metadata.h"
#include "factory_cpu_result_pool.h"
#ifdef FACTORY_JPEG_REFERENCE_SHARPEN
static __attribute__((noreturn)) void factory_sharpen_join_failure(void){
 text("JPEG_SHARPEN_THREAD_JOIN_REJECTED\n");sc(94,76,0,0,0);__builtin_unreachable();
}
#include "factory_reference_sharpen.h"
#endif
typedef struct {int fd;const uint8_t *pixels;} MemoryImageSource;
static int memory_image_read(void *context,uint64_t offset,unsigned char *out,uint32_t bytes){
 MemoryImageSource *source=context;
 const uint64_t length=(uint64_t)CBW_WIDTH*CBW_HEIGHT*2;int fd=source->fd;
 if(!out||offset>length||bytes>length-offset)return 0;
 if(source->pixels){memcpy(out,source->pixels+offset,bytes);return 1;}
 while(bytes){long n=jpeg_sc(67,fd,(long)out,bytes,(long)(16384+offset),0,0);
  if(n==-4)continue;if(n<=0||(uint64_t)n>bytes)return 0;out+=n;offset+=n;bytes-=(uint32_t)n;}
 return 1;
}
static int memory_merged_open(void){
 char path[128];unsigned n=0;while(candidate_raw_path[n]&&n<sizeof path)n++;
 if(n<12||n>=sizeof path||!same(candidate_raw_path+n-12,"/memory.sock"))return -1;
 unsigned parent=n-12;const char *tail="/merged.3FR";
 for(unsigned i=0;i<parent;i++)path[i]=candidate_raw_path[i];
 unsigned k=0;do{path[parent+k]=tail[k];}while(tail[k++]);
 return (int)sc(56,-100,(long)path,0xA0000,0);
}
static int memory_merged_wait(int fd,unsigned rows){
 uint64_t wanted=16384+(uint64_t)rows*CBW_WIDTH*2;
 for(unsigned i=0;i<400;i++){
  long bytes=sc(62,fd,0,2,0);if(bytes<0)return 0;
  if((uint64_t)bytes>=wanted)return 1;
  long delay[2]={0,50000000};if(sc(101,(long)delay,0,0,0))return 0;
 }
 return 0;
}
static int render_memory_bands(void *bus,void *event,void *ion){
 void *raw=0,*yuv=0,*full=0,*raw_map=0,*yuv_map=0,*full_map=0,*request=0,*reply=0;
 uint64_t handles[2]={0},identities[2]={0};uint32_t raw_size=0,yuv_size=0,full_size=0;
 FactoryBufferGeometry graw,gyuv,gfull;
 int merged=-1;uint8_t header[F3_HEADER];
 uint16_t cache0[CBW_WIDTH],cache1[CBW_WIDTH];int rc=70;
 void *native_event=0;uint8_t *metadata=0;uint64_t metadata_bytes=0;
 void *result=0,*result_map=0;MemoryImageSource image={.fd=-1};
 const char *phase="OPEN";unsigned native_index=0;
 if(!memory_open()||!factory_buffer_geometry(&graw,11836,8842,1)||
    !factory_buffer_geometry(&gyuv,11656,8742,6)||
    !factory_buffer_geometry(&gfull,CBW_WIDTH,CBW_HEIGHT,6)||factory_native_group(bus)!=255)goto done;
 CbwPacket hello;if(!memory_transfer(memory_peer,&hello,sizeof hello,0))goto done;
 phase="METADATA";
 if(!event){
  if(!
#ifdef FACTORY_MEMORY_FULL_RESULT
     cbw_full_hello(&hello)
#else
     cbw_native_hello(&hello)
#endif
     )goto done;
  metadata_bytes=hello.bytes;
  long address=jpeg_sc(222,0,(long)metadata_bytes,3,0x22,-1,0);
  if(address<0&&address>=-4095)goto done;metadata=(uint8_t*)address;
  if(!memory_transfer(memory_peer,metadata,metadata_bytes,0)||
     !crm_rawpb_wb_valid(metadata,(size_t)metadata_bytes)||
     !(native_event=memory_request_template(metadata,(unsigned)metadata_bytes)))goto done;
  event=native_event;
  if(sc(215,(long)metadata,(long)metadata_bytes,0,0))goto done;metadata=0;
 }else if(!cbw_hello(&hello))goto done;
#ifdef FACTORY_MEMORY_FULL_RESULT
 if(!native_event)goto done;
#endif
 phase="MERGED_SOURCE";
 merged=memory_merged_open();
 if(merged<0||!memory_merged_wait(merged,0)||
    jpeg_sc(67,merged,(long)header,sizeof header,0,0,0)!=sizeof header||
    !memory_native_header(header))goto done;
 unsigned next=0,native_row=0;uint64_t ticket=0;
 CbwPacket finish;
#ifdef FACTORY_MEMORY_FULL_RESULT
 phase="FULL_RESULT_ALLOCATE";
 uint32_t result_size=0;uint64_t result_handle=0;
 if(duss_hal_mem_alloc(ion,&result,(uint32_t)CBW_FULL_BYTES,1024,FACTORY_CPU_RESULT_HEAP,FACTORY_TASK_ION_FLAGS)||!result||
    duss_hal_mem_get_size(result,&result_size)||result_size<CBW_FULL_BYTES||
    result_size>((CBW_FULL_BYTES+4095)&~UINT64_C(4095))||
    duss_hal_mem_share(result,&result_handle)||!result_handle)goto done;
 text("MEMORY_CPU_FULL_RESULT_HEAP2_ALLOCATED_SHARED\n");
 CbwSharedResult shared={CBW_MAGIC,hello.task,result_handle,CBW_FULL_BYTES};
 if(!memory_ack(&hello)||!memory_transfer(memory_peer,&shared,sizeof shared,1))goto done;
 phase="FULL_RESULT_WAIT";CbwPacket ready;
 if(!memory_transfer(memory_peer,&ready,sizeof ready,0)||!cbw_full_ready(&ready,hello.task)||
    duss_hal_mem_map(result,&result_map)||!result_map||duss_hal_mem_sync(result,FACTORY_SYNC_CPU))goto done;
 image.pixels=result_map;next=CBW_HEIGHT;ticket=CBW_FILE_BATCHES;
 if(!memory_ack(&ready))goto done;
 text("MEMORY_FULL_RESULT_READY_SOURCE_RELEASED_RAW_WRITE_AND_RENDER_PARALLEL\n");
#else
 if(!memory_ack(&hello))goto done;
 for(unsigned index=0;index<CBW_FILE_BATCHES;index++){
  phase="INPUT";CbwPacket packet;
  if(!memory_transfer(memory_peer,&packet,sizeof packet,0)||!cbw_file_band(&packet,hello.task,index,next,ticket))goto done;
  next+=packet.rows;ticket=packet.ticket;
  /* RAW 消费者独立写入同一个必需成片。未写齐不能读取，ACK 不等同成片完成。 */
  if(!memory_merged_wait(merged,next)||!memory_ack(&packet))goto done;
  text("MEMORY_INPUT_AND_RAW_WRITE_DONE ");number(index);text("\n");
 }
 phase="SOURCE_RELEASE_CONFIRMATION";
 if(!memory_transfer(memory_peer,&finish,sizeof finish,0))goto done;
 CbwPacket expected={.magic=CBW_MAGIC,.task=hello.task,.ticket=ticket,
  .operation=CBW_FINISH,.index=CBW_FILE_BATCHES,.first_row=CBW_HEIGHT,.width=CBW_WIDTH};
 if(memcmp(&finish,&expected,sizeof finish)||next!=CBW_HEIGHT)goto done;
 text("MEMORY_SOURCE_RELEASE_CONFIRMED_BEFORE_WORKSPACE\n");
#endif
 image.fd=merged;
 phase="NATIVE_ALLOCATE";
 uint64_t allocation_begin=jpeg_monotonic_ms();
 if(duss_hal_mem_alloc(ion,&full,gfull.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
    duss_hal_mem_alloc(ion,&raw,graw.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
    duss_hal_mem_alloc(ion,&yuv,gyuv.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||
    duss_hal_mem_get_size(raw,&raw_size)||duss_hal_mem_get_size(yuv,&yuv_size)||
    duss_hal_mem_get_size(full,&full_size)||!factory_buffer_fits(&graw,raw_size)||
    !factory_buffer_fits(&gyuv,yuv_size)||!factory_buffer_fits(&gfull,full_size)||
    duss_hal_mem_share(raw,&handles[0])||duss_hal_mem_share(yuv,&handles[1])||
    duss_hal_mem_get_phys_addr(raw,&identities[0])||duss_hal_mem_get_phys_addr(yuv,&identities[1])||
    !identities[0]||!identities[1]||identities[0]==identities[1]||
    duss_hal_mem_map(full,&full_map)||!full_map)goto done;
 jpeg_elapsed("TILE_ALLOCATION_MS ",allocation_begin,jpeg_monotonic_ms());
 for(unsigned plane=0;plane<2;plane++)for(unsigned row=0;row<gfull.height;row++)
  memset((uint8_t*)full_map+gfull.planes[plane].offset+(uint64_t)row*gfull.stride_pixels+gfull.width,
    0,gfull.stride_pixels-gfull.width);
  while(native_row<CBW_HEIGHT&&memory_native_window_ready(next,native_row)){
   unsigned oy=native_row;
   for(unsigned ox=0;ox<CBW_WIDTH;ox+=gyuv.width-2){
  unsigned width=CBW_WIDTH-ox,height=CBW_HEIGHT-oy;
  if(width>gyuv.width-2)width=gyuv.width-2;if(height>gyuv.height)height=gyuv.height;
  phase="RAW_FILL";
  uint64_t fill_begin=jpeg_monotonic_ms();
  if(duss_hal_mem_map(raw,&raw_map)||!raw_map||
     !factory_raw_tile(memory_image_read,&image,raw_map,CBW_WIDTH,CBW_HEIGHT,CBW_WIDTH,
       graw.width,graw.height,graw.stride_pixels,ox,oy,129,96,cache0,cache1))goto done;
  memset((uint8_t*)raw_map+graw.content_bytes,0,raw_size-graw.content_bytes);
  if(duss_hal_mem_sync(raw,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(raw))goto done;raw_map=0;
  jpeg_elapsed("TILE_RAW_PREPARE_MS ",fill_begin,jpeg_monotonic_ms());
  FactoryBufferGeometry geometry[2]={graw,gyuv};uint64_t allocated[2]={raw_size,yuv_size};
  request=rebuild_request_sized(event,handles,geometry,allocated);if(!request)goto done;
  if(!dbus_message_set_sender(request,0)||!dbus_message_set_destination(request,"com.hasselblad.camera")||
     !factory_route_publish(identities))goto done;
  phase="NATIVE_RPC";memory_native_submitted=1;
  text("TILE_SUBMIT ");number(native_index);text("\n");
  uint64_t begin=jpeg_monotonic_ms();
  reply=dbus_connection_send_with_reply_and_block(bus,request,20000,0);
  jpeg_elapsed("TILE_NATIVE_REQUEST_MS ",begin,jpeg_monotonic_ms());
  dbus_message_unref(request);request=0;
  if(!reply){text("MEMORY_NATIVE_NO_REPLY\n");goto done;}
  if(dbus_message_get_type(reply)!=2){
   text("MEMORY_NATIVE_REPLY_ERROR ");const char *name=dbus_message_get_error_name(reply);
   if(name)text(name);text("\n");goto done;
  }
  if(!factory_route_acknowledged()){text("MEMORY_NATIVE_BRANCH_ACK_MISSING\n");goto done;}
  Iter result={0};if(dbus_message_iter_init(reply,&result))goto done;
  dbus_message_unref(reply);reply=0;
  phase="NATIVE_STOP";
  if(!factory_stop_native(bus)||!factory_route_clear())goto done;
  memory_native_submitted=0;phase="YUV_COPY";
  uint64_t assembly_begin=jpeg_monotonic_ms();
  if(duss_hal_mem_map(yuv,&yuv_map)||!yuv_map||duss_hal_mem_sync(yuv,FACTORY_SYNC_CPU)||
     !factory_tile_copy((uint8_t*)yuv_map+gyuv.planes[0].offset,
       (uint8_t*)yuv_map+gyuv.planes[1].offset,gyuv.width,gyuv.height,gyuv.stride_pixels,1,
       (uint8_t*)full_map+gfull.planes[0].offset,(uint8_t*)full_map+gfull.planes[1].offset,
       gfull.width,gfull.height,gfull.stride_pixels,ox,oy,width,height)||duss_hal_mem_unmap(yuv))goto done;
  jpeg_elapsed("TILE_ASSEMBLE_MS ",assembly_begin,jpeg_monotonic_ms());
  yuv_map=0;text("TILE_COPIED ");number(native_index++);text("\n");
 }
   native_row+=gyuv.height;
  }
 if(native_index!=9)goto done;
 phase="JPEG";
 if(result_map){if(duss_hal_mem_unmap(result))goto done;result_map=0;image.pixels=0;}
 if(result){if(duss_hal_mem_free(result))goto done;result=0;}
 if(sc(57,merged,0,0,0))goto done;merged=-1;
 if(duss_hal_mem_free(raw))goto done;raw=0;
 if(duss_hal_mem_free(yuv))goto done;yuv=0;
#ifdef FACTORY_JPEG_REFERENCE_SHARPEN
 phase="JPEG_SHARPEN";
 {
  uint64_t sharpen_begin=jpeg_monotonic_ms();
  if(!factory_reference_sharpen((uint8_t*)full_map+gfull.planes[0].offset,
       gfull.width,gfull.height,gfull.stride_pixels))goto done;
  jpeg_elapsed("JPEG_SHARPEN_MS ",sharpen_begin,jpeg_monotonic_ms());
  text("JPEG_REFERENCE_SHARPEN_APPLIED amount=");number(FS_AMOUNT);
  text(" radius_tenths=");number(FS_RADIUS_TENTHS);
  text(" threshold=1 halo=0 threads=");number(FS_THREADS);text("\n");
 }
#endif
 if(duss_hal_mem_sync(full,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(full))goto done;full_map=0;
#ifdef FACTORY_MEMORY_FULL_RESULT
 {
  phase="RAW_WRITE_FINISH_CONFIRMATION";
  CbwPacket expected={.magic=CBW_MAGIC,.task=hello.task,.ticket=ticket,
   .operation=CBW_FINISH,.index=CBW_FILE_BATCHES,.first_row=CBW_HEIGHT,.width=CBW_WIDTH};
  if(!memory_transfer(memory_peer,&finish,sizeof finish,0)||memcmp(&finish,&expected,sizeof finish))goto done;
  text("MEMORY_SHARED_RAW_ALL_CONSUMERS_RELEASED_BEFORE_JPEG\n");
 }
#endif
 phase="JPEG_ENCODE_AND_WRITE";
 rc=encode_jpeg_sized(ion,full,CBW_WIDTH,CBW_HEIGHT,gfull.stride_pixels);
 if(!rc)memory_finish=finish;
done:
 if(rc){text("MEMORY_FAILURE_PHASE ");text(phase);text(" TILE ");number(native_index);text("\n");}
 if(memory_native_submitted){
  if(!factory_stop_native(bus)){
   rc=75;text("MEMORY_NATIVE_DRAIN_DEFERRED_BUFFERS_RETAINED\n");
   if(memory_peer>=0)sc(210,memory_peer,2,0,0);
   while(!factory_wait_native_idle(bus,20)){long delay[2]={1,0};sc(101,(long)delay,0,0,0);}
  }
  memory_native_submitted=0;
 }
 if(!factory_route_clear())rc=75;
 if(native_event)dbus_message_unref(native_event);
 if(metadata&&sc(215,(long)metadata,(long)metadata_bytes,0,0))rc=72;
 if(reply)dbus_message_unref(reply);if(request)dbus_message_unref(request);
 if(raw_map&&duss_hal_mem_unmap(raw))rc=71;if(yuv_map&&duss_hal_mem_unmap(yuv))rc=71;
 if(full_map&&duss_hal_mem_unmap(full))rc=71;
 if(result_map&&duss_hal_mem_unmap(result))rc=71;
 if(result&&duss_hal_mem_free(result))rc=72;
 if(full&&duss_hal_mem_free(full))rc=72;if(yuv&&duss_hal_mem_free(yuv))rc=72;
 if(raw&&duss_hal_mem_free(raw))rc=72;
 if(merged>=0&&sc(57,merged,0,0,0))rc=72;
 return rc;
}
