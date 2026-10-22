/* 完整 JPEG 编码候选：256 行的原厂编码任务，重启标记拼为一张整幅图。
 * 原厂显影和整幅锐化不变；只在已有完整 YUV 上进行，不重拍、不重新显影。 */
#include "factory_jpeg_band_header.h"
#define FJB_ROWS 256u
#ifndef FACTORY_JPEG_BAND_CAPACITY
#define FACTORY_JPEG_BAND_CAPACITY FACTORY_JPEG_CAPACITY
#endif
#define FJB_OUTPUT_CAPACITY FACTORY_JPEG_BAND_CAPACITY
static int fjb_write(int fd,const uint8_t *p,uint32_t n){
 while(n){long k=jpeg_sc(64,fd,(long)p,n,0,0,0);if(k==-4)continue;if(k<=0||(uint64_t)k>n)return 0;p+=k;n-=(uint32_t)k;}return 1;
}
static int encode_jpeg_bands_to(void *ion,void *input,uint32_t width,uint32_t height,uint32_t stride,const char *path){
 void *source=0,*band=0,*band_map=0,*output=0,*out_map=0,*encoder=0;
 int rc=80,fd=-1,owned=0;uint32_t saved_bytes=0;
 uint8_t first[FJB_HEADER_MAX],header[FJB_HEADER_MAX];FjbHeader initial={0};
 uint64_t plane=(uint64_t)stride*height,band_bytes=(uint64_t)stride*FJB_ROWS*2;
 if(width!=23326||height!=17498||stride!=23552||!path||band_bytes>UINT32_MAX)goto end;
 if(duss_hal_mem_map(input,&source)||!source||duss_hal_mem_sync(input,FACTORY_SYNC_CPU))goto end;
 rc=81;if(duss_hal_mem_alloc(ion,&band,(uint32_t)band_bytes,1024,23,FACTORY_TASK_ION_FLAGS)||!band||
    duss_hal_mem_alloc(ion,&output,FJB_OUTPUT_CAPACITY,1024,23,FACTORY_TASK_ION_FLAGS)||!output)goto end;
 jpeg_msg("NATIVE_JPEG_BAND_CAPACITY bytes=");jpeg_num(FJB_OUTPUT_CAPACITY);jpeg_msg("\n");
 jpeg_msg("NATIVE_JPEG_QUALITY value=");jpeg_num(FACTORY_JPEG_NATIVE_QUALITY);jpeg_msg("\n");
 uint64_t begin=jpeg_monotonic_ms();
 for(uint32_t row=0,index=0;row<height;row+=FJB_ROWS,index++){
  unsigned rows=height-row;if(rows>FJB_ROWS)rows=FJB_ROWS;
  uint32_t bytes=stride*rows;
  rc=82;if(duss_hal_mem_map(band,&band_map)||!band_map)goto end;
  for(unsigned p=0;p<2;p++){
   const uint64_t *from=(const uint64_t*)((uint8_t*)source+p*plane+(uint64_t)row*stride);
   uint64_t *to=(uint64_t*)((uint8_t*)band_map+p*bytes);
   for(uint32_t k=0;k<bytes/8;k++)to[k]=from[k];
  }
  if(duss_hal_mem_sync(band,FACTORY_SYNC_DEVICE)||duss_hal_mem_unmap(band))goto end;band_map=0;
  rc=83;if(duss_hal_device_open("/dev/ienc0",0,&encoder)||!encoder)goto end;
  unsigned char frame[0x218],stream[0x1c8];
  if(factory_jpeg_layout_sized(frame,stream,band,output,FJB_OUTPUT_CAPACITY,width,rows,stride))goto end;
  struct {void *frame,*stream;uint32_t quality,reserved;} request={frame,stream,FACTORY_JPEG_NATIVE_QUALITY,0};
  int native=duss_hal_ienc_encfrm(encoder,&request);
  if(native){jpeg_msg("NATIVE_JPEG_BAND_ENCODER_FAILED\n");goto end;}
  if(duss_hal_device_close(encoder))goto end;encoder=0;
  uint32_t offset=factory_u32(stream+0x3c),extra=factory_u32(stream+0x44),length=factory_u32(stream+0x40);
  rc=84;if(offset>FJB_OUTPUT_CAPACITY||extra>FJB_OUTPUT_CAPACITY-offset||length<4||length>FJB_OUTPUT_CAPACITY-offset-extra||
     duss_hal_mem_map(output,&out_map)||!out_map||duss_hal_mem_sync(output,FACTORY_SYNC_CPU))goto end;
  const uint8_t *jpg=(uint8_t*)out_map+offset+extra;FjbHeader parsed;
  rc=85;if(!fjb_parse(jpg,length,&parsed)||parsed.width!=width||parsed.height!=rows)goto end;
  if(!index){
   initial=parsed;for(unsigned n=0;n<parsed.scan;n++)first[n]=jpg[n];
   int header_bytes=fjb_first_header(header,sizeof header,jpg,&parsed,height,FJB_ROWS);if(header_bytes<1)goto end;
   rc=86;fd=(int)jpeg_sc(56,-100,(long)path,193,0600,0,0);if(fd<0)goto end;owned=1;
   if(!fjb_write(fd,header,(uint32_t)header_bytes))goto end;
  }else{
   rc=85;if(!fjb_same_header(first,&initial,jpg,&parsed))goto end;
   const uint8_t restart[2]={255,(uint8_t)(208+(index-1)%8)};
   rc=86;if(!fjb_write(fd,restart,sizeof restart))goto end;
  }
  rc=86;if(!fjb_write(fd,jpg+parsed.scan,length-parsed.scan-2))goto end;
  if(duss_hal_mem_unmap(output))goto end;out_map=0;
  jpeg_msg("NATIVE_JPEG_BAND_DONE ");jpeg_num(index);jpeg_msg("\n");
 }
 {
  const uint8_t end_marker[2]={255,217};rc=86;
  if(!fjb_write(fd,end_marker,sizeof end_marker))goto end;
  long length=jpeg_sc(62,fd,0,1,0,0,0);if(length<4||(uint64_t)length>UINT32_MAX)goto end;
  saved_bytes=(uint32_t)length;
#ifdef FACTORY_JPEG_BAND_FINAL_FILE
  /* 自造检查器直接输出最终文件。拍摄路径在加入 EXIF/预览后由 pack
   * 对齐 EOF，中间 render.jpg 必须以 EOI 结束，不能提前补尾。 */
  const uint8_t zeros[4096]={0};unsigned pad=(unsigned)((4096u-((uint64_t)length&4095u))&4095u);
  if(pad&&!fjb_write(fd,zeros,pad))goto end;
  jpeg_msg("NATIVE_JPEG_BANDS_EOF_4K_ALIGNED\n");
#endif
  if(jpeg_sc(82,fd,0,0,0,0,0))goto end;
 }
 jpeg_elapsed("NATIVE_JPEG_BANDS_ENCODE_AND_WRITE_MS ",begin,jpeg_monotonic_ms());
 jpeg_msg("NATIVE_JPEG_FULL_FRAME_RESTART_BANDS_COMPLETE\n");rc=0;
end:
 if(fd>=0&&jpeg_sc(57,fd,0,0,0,0,0))rc=86;
 if(out_map&&duss_hal_mem_unmap(output))rc=87;
 if(band_map&&duss_hal_mem_unmap(band))rc=87;
 if(encoder&&duss_hal_device_close(encoder))rc=88;
 if(output&&duss_hal_mem_free(output))rc=89;
 if(band&&duss_hal_mem_free(band))rc=89;
 if(source&&duss_hal_mem_unmap(input))rc=87;
 if(rc&&owned)jpeg_sc(35,-100,(long)path,0,0,0,0);
 /* 保留运行脚本、调度器使用的原有完成协议。仅 fsync、关闭和引用清理
  * 均成功后输出；任一分段或清理失败都没有成功标记。 */
 if(!rc){jpeg_msg("NATIVE_JPEG_SAVED bytes=");jpeg_num(saved_bytes);jpeg_msg("\n");}
 return rc;
}
