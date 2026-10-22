/* 4.2.0 十六张磁盘分支：导出本次第一帧真实显影 metadata。
 * 存储 owner 线程持有 FileContent 时完成 WB 转换；不读取参考照片。 */
#include "capture_metadata_native.h"
#include "capture_header_native.h"
static CmNative disk_native;
static ChNative disk_read;
static ChReadApi disk_read_api;
static ChMetadata disk_metadata;
static int disk_metadata_written;
static int disk_prepare(void *service){
 CmApi input;
 return cm_native_bind(&disk_native,base,service,&input)&&
  ch_native_open(&disk_read,&disk_native,&disk_read_api);
}
static int disk_metadata_receive(CaptureFrame *frame,const X2dExposureInfo *info){
 if(frame->index||info->kind!=1)return 1;
 /* 重复 RAW 通知由原帧状态拒绝，不能用它覆盖首帧 metadata。 */
 return !disk_metadata.bytes&&
  ch_metadata_copy(&disk_native,disk_read_api,&info->metadata_map,&disk_metadata);
}
static int disk_metadata_export(CaptureFrame *frame){
 if(frame->index)return 1;
 if(disk_metadata_written)return 1;
 if(!frame->frame.content.value){log_line("SIXTEEN_NATIVE_METADATA_CONTENT_MISSING\n");return 0;}
 if(!disk_metadata.bytes){log_line("SIXTEEN_NATIVE_METADATA_CONTAINER_MISSING\n");return 0;}
 if(!ch_metadata_write_rawpb(&disk_native,disk_read_api,frame->frame.content.value,&disk_metadata)){
  log_line("SIXTEEN_NATIVE_METADATA_WHITE_BALANCE_FAILED\n");return 0;
 }
 char path[256];int n=snprintf(path,sizeof path,"%s/rawpb.bin",X2D_PRIVATE_CAPTURE_JOB);
 if(n<=0||(size_t)n>=sizeof path)return 0;
 int fd=open(path,O_CREAT|O_EXCL|O_NOFOLLOW|O_WRONLY,0600);if(fd<0)return 0;
 size_t offset=0;int ok=1;
 while(offset<disk_metadata.size){
  ssize_t written=write(fd,disk_metadata.bytes+offset,disk_metadata.size-offset);
  if(written<=0||(size_t)written>disk_metadata.size-offset){ok=0;break;}
  offset+=(size_t)written;
 }
 if(fsync(fd))ok=0;if(close(fd))ok=0;
 if(!ok)return 0;
 disk_metadata_written=1;ch_metadata_destroy(&disk_metadata);
 log_line("SIXTEEN_NATIVE_RAWPB_WHITE_BALANCE_WRITTEN\n");return 1;
}

/* 首写 Work 在 owner 线程就绪时提取完整元数据；FileIO 完成后只检查交付标记，
 * 不再依赖可能已被原厂收回的 WriteFileContent metadata。 */
static void disk_observe_work(void *context,X2dPrivateFrame *native,const X2dQtShared *work){
 CaptureFrame *frame=context;
 if(!frame||native!=&frame->frame||!work||!work->value||!work->control){
  failure("SIXTEEN_NATIVE_METADATA_WORK_REJECTED\n");return;
 }
 if(!disk_metadata_export(frame))failure("SIXTEEN_NATIVE_METADATA_BEFORE_WRITE_FAILED\n");
}
static int disk_metadata_complete(CaptureFrame *frame){
 return frame->index||disk_metadata_written;
}
