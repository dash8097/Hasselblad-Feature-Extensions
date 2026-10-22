/* 4.2.0：原厂单帧显影完成后四线程缩小，编码仍走原厂80/64MiB。 */
#ifndef FACTORY_QUARTER_THREAD_EXTERNAL
#include <pthread.h>
#endif
#include "quarter_single_core.c"
typedef struct {
 const uint8_t *input;uint8_t *output;
 FactoryBufferGeometry source,target;unsigned first,rows;int ok,finished;
} QuarterRenderWork;
static void *quarter_render_rows(void *context){
 QuarterRenderWork *w=context;
 w->ok=quarter_nv16_rows(w->input,w->source.allocation_bytes,w->source.width,w->source.height,
  w->source.stride_pixels,w->output,w->target.allocation_bytes,w->target.width,QUARTER_HEIGHT,
  w->target.stride_pixels,w->target.height,w->first,w->rows);
 __atomic_store_n(&w->finished,1,__ATOMIC_RELEASE);return 0;
}
static int quarter_render_small(void *ion,void **full,FactoryBufferGeometry *g){
 FactoryBufferGeometry small;void *buffer=0,*input=0,*output=0;int rc=79;
 pthread_t threads[3];unsigned started=0;QuarterRenderWork work[4];int join_failed=0;
 if(!factory_buffer_geometry(&small,QUARTER_WIDTH,(QUARTER_HEIGHT+1)&~1u,6)||
    duss_hal_mem_alloc(ion,&buffer,small.allocation_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||!buffer||
    duss_hal_mem_map(*full,&input)||!input||duss_hal_mem_sync(*full,FACTORY_SYNC_CPU)||
    duss_hal_mem_map(buffer,&output)||!output)goto done;
 for(unsigned i=0;i<4;i++){
  unsigned first=(uint64_t)QUARTER_HEIGHT*i/4,end=(uint64_t)QUARTER_HEIGHT*(i+1)/4;
  work[i]=(QuarterRenderWork){input,output,*g,small,first,end-first,0,0};
 }
 for(unsigned i=0;i<3;i++){
  if(pthread_create(threads+i,0,quarter_render_rows,work+i))break;
  started++;
 }
 for(unsigned i=started;i<4;i++)quarter_render_rows(work+i);
 for(unsigned i=0;i<started;i++)if(pthread_join(threads[i],0)){
  /* join 异常也要等该工人结束读写，不能直接释放它仍在用的映射。 */
  while(!__atomic_load_n(&work[i].finished,__ATOMIC_ACQUIRE)){
   long delay[2]={0,1000000};sc(101,(intptr_t)delay,0,0,0);
  }
  join_failed=1;
 }
 if(join_failed)goto done;
 for(unsigned i=0;i<4;i++)if(!work[i].ok)goto done;
 if(small.height>QUARTER_HEIGHT)for(unsigned p=0;p<2;p++){
  uint8_t *last=(uint8_t*)output+small.planes[p].offset+(uint64_t)(QUARTER_HEIGHT-1)*small.stride_pixels;
  memcpy(last+small.stride_pixels,last,small.stride_pixels);
 }
 if(duss_hal_mem_unmap(*full))goto done;input=0;
 if(duss_hal_mem_sync(buffer,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(buffer))goto done;output=0;
 /* 原厂显影已停止；释放一亿YUV后，编码缓冲仅与2500万YUV共存。 */
 if(duss_hal_mem_free(*full))goto done;
 *full=buffer;buffer=0;*g=small;rc=0;
 text("QUARTER_NATIVE_SINGLE_RENDER_4_WORKERS_5828_4371_READY\n");
done:
 if(input&&duss_hal_mem_unmap(*full))rc=79;if(output&&duss_hal_mem_unmap(buffer))rc=79;
 if(buffer&&duss_hal_mem_free(buffer))rc=79;return rc;
}
/* 编码器内部补一行；SOF恢复真实4371行。MCU行数相同，无解码或二次压缩。 */
static int stack_read(int,void*,uint32_t,uint64_t);
static int quarter_jpeg_visible_height(const char *path){
 int fd=(int)sc(56,-100,(intptr_t)path,0xA0002,0),ok=0;
 if(fd<0)return 0;uint8_t b[9];uint64_t at=2;
 if(!stack_read(fd,b,2,0)||b[0]!=255||b[1]!=216)goto done;
 for(unsigned i=0;i<128;i++){
  if(!stack_read(fd,b,4,at)||b[0]!=255||b[1]==0xda)goto done;
  unsigned length=((unsigned)b[2]<<8)|b[3];if(length<2)goto done;
  if(b[1]==0xc0){
   if(!stack_read(fd,b,9,at)||b[4]!=8||(((unsigned)b[5]<<8)|b[6])!=((QUARTER_HEIGHT+1)&~1u)||
      (((unsigned)b[7]<<8)|b[8])!=QUARTER_WIDTH||
      (QUARTER_HEIGHT+7)/8!=(((QUARTER_HEIGHT+1)&~1u)+7)/8)goto done;
   b[5]=(uint8_t)(QUARTER_HEIGHT>>8);b[6]=(uint8_t)QUARTER_HEIGHT;
   if(jpeg_sc(68,fd,(intptr_t)(b+5),2,at+5,0,0)!=2||sc(82,fd,0,0,0))goto done;
   ok=1;break;
  }
  at+=2+length;
 }
done:
 if(sc(57,fd,0,0,0))ok=0;return ok;
}
