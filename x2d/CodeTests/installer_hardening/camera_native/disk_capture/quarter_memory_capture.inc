#include "quarter_memory_file.h"
#include "quarter_memory_wire.h"
static unsigned quarter_pending;
static uint64_t quarter_shared_at;
static int quarter_binary(const char *name,const void *data,size_t size){
 char p[256];if(!path(p,sizeof p,name))return 0;
 int fd=open(p,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return 0;
 const uint8_t *b=data;int ok=1;
 while(size){ssize_t n=write(fd,b,size);if(n<=0||(size_t)n>size){ok=0;break;}b+=n;size-=(size_t)n;}
 if(fsync(fd)||close(fd))ok=0;return ok;
}
static int quarter_output(const RsPixelBlock *blocks,unsigned count){
 char p[256];QmWire wire={.magic=QM_MAGIC,.count=count};
 int (*share)(void*,uint64_t*)=dlsym(RTLD_DEFAULT,"duss_hal_mem_share");
 int (*sync_mem)(void*,uint32_t)=dlsym(RTLD_DEFAULT,"duss_hal_mem_sync");
 if(core.frames!=1||core.config.method!=RS_MAXIMUM||core.config.noise_group||count!=core.segment_count||
    !share||!sync_mem||!rs_async_shutdown(&accumulating)||!path(p,sizeof p,"merged.3FR")||
    !qm_raw_write(p,first_header,blocks,count,preview,preview_bytes)||!metadata_file()||
    !quarter_binary("source.header",first_header,sizeof first_header))return 0;
 for(unsigned i=0;i<count;i++){
  uint32_t actual=0;
  if(blocks[i].pixels!=(const uint16_t*)mapped[i]||blocks[i].bytes>UINT32_MAX||
     mem_size(accumulator[i],&actual)||sync_mem(accumulator[i],2)||share(accumulator[i],&wire.segment[i].handle))return 0;
  wire.segment[i].bytes=(uint32_t)blocks[i].bytes;wire.segment[i].allocation=actual;
 }
 if(!qm_wire_valid(&wire)||!quarter_binary("raw.memory",&wire,sizeof wire)||
    !put("quarter.memory.ready","QUARTER_MEMORY_R1\n",1))return 0;
 quarter_shared_at=now_ms();quarter_pending=1;
 log_line("QUARTER_FINAL_RAW_WRITTEN_SOURCE_REMAINS_ONLY_IN_SHARED_MEMORY\n");return 1;
}
static void quarter_drain(void){
 char marker[32];
 if(text_read("quarter.memory.copied",marker,sizeof marker)==18&&!strcmp(marker,"QUARTER_COPIED_R1\n")){
  if(!pool_release()||!put("raw.ready","RAW_STACK_R1\n",1)){failure("QUARTER_MEMORY_RELEASE_REJECTED\n");return;}
  output_done=1;quarter_pending=0;log_line("QUARTER_SOURCE_RELEASED_AFTER_RENDER_COPY_NO_SOURCE_RAW_DISK\n");
 }else if(now_ms()-quarter_shared_at>180000)failure("QUARTER_MEMORY_CONSUMER_TIMEOUT\n");
}
