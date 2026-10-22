/* 只更新自有任务合成 RAW 的预览；不创建或读取原片。 */
#define _POSIX_C_SOURCE 200809L
#include "capture_runtime_job.h"
#include "capture_preview_stream.h"
#include "raw_stack_file.h"
#include "raw_stack_iso.h"
#include "photo_file_alignment.h"
#ifdef RAW_STACK_QUARTER_FINAL
#include "factory_native_3fr.h"
#define FINAL_RAW_BYTES 52627584u
static int final_header(const uint8_t *source,size_t n,uint8_t *updated){
 memcpy(updated,source,16384);uint32_t main=f3_u32(updated+4),raw;
 return n&&n<=16u*1024*1024&&!memcmp(updated,"II*\0",4)&&f3_scalar(updated,16384,main,330,&raw)&&
  f3_is(updated,16384,raw,256,5952)&&f3_is(updated,16384,raw,257,4421)&&
  f3_is(updated,16384,raw,273,16384)&&f3_is(updated,16384,raw,279,FINAL_RAW_BYTES)&&
  f3_put32(updated,main,273,16384+FINAL_RAW_BYTES)&&f3_put32(updated,main,279,(uint32_t)n);
}
#else
#define FINAL_RAW_BYTES 210510336u
#define final_header rs_raw_header
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
static int write_at(int fd,const void *data,size_t bytes,off_t offset){
 const uint8_t *p=data;while(bytes){ssize_t n=pwrite(fd,p,bytes,offset);if(n<=0||(size_t)n>bytes)return 0;p+=n;bytes-=(size_t)n;offset+=n;}return 1;
}
int main(int argc,char **argv){
 char job[108],path[192];uint8_t header[16384],updated[16384];int rc=2,raw=-1,preview=-1;uint8_t *bytes=NULL;
 int iso_mode=argc==3&&!strcmp(argv[2],"--iso");RsIsoResult iso_result;
 if((argc!=2&&!iso_mode)||!capture_runtime_job(job,argv[1]))return rc;
 if(iso_mode){
  snprintf(path,sizeof path,"%s/iso.result",job);int fd=open(path,O_RDONLY|O_NOFOLLOW);char record[160];
  if(fd<0)return rc;struct stat state;ssize_t count;
  if(fstat(fd,&state)||!S_ISREG(state.st_mode)||state.st_size<=0||state.st_size>=(off_t)sizeof record){close(fd);return rc;}
  count=read(fd,record,sizeof record-1);close(fd);if(count!=state.st_size)return rc;record[count]=0;
  if(!rs_iso_record(record,&iso_result))return rc;
 }
 snprintf(path,sizeof path,"%s/preview.jpg",job);preview=open(path,O_RDONLY|O_NOFOLLOW);if(preview<0)goto done;
 struct stat st;if(fstat(preview,&st)||!S_ISREG(st.st_mode)||st.st_size<4||st.st_size>16*1024*1024)goto done;
 size_t n=(size_t)st.st_size;bytes=malloc(n);if(!bytes||read(preview,bytes,n)!=(ssize_t)n||capture_preview_stream_size(bytes,n)!=n)goto done;
 snprintf(path,sizeof path,"%s/merged.3FR",job);raw=open(path,O_RDWR|O_NOFOLLOW);if(raw<0)goto done;
 if(fstat(raw,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||st.st_size<(off_t)(16384+FINAL_RAW_BYTES)||
    pread(raw,header,sizeof header,0)!=sizeof header||!final_header(header,n,updated)){fprintf(stderr,"RAW_STACK_FINAL_HEADER_REJECTED\n");goto done;}
 if(iso_mode&&!rs_iso_stamp(updated,&iso_result,updated)){fprintf(stderr,"ISO_STACK_FINAL_METADATA_REJECTED\n");goto done;}
 off_t end=16384+FINAL_RAW_BYTES+(off_t)n;size_t padding=photo_eof_padding((uint64_t)end);
 const uint8_t zeros[4096]={0};
 /* 预览缩短时也明确清零，不能保留旧预览尾部作为填充。 */
 if(!write_at(raw,bytes,n,16384+FINAL_RAW_BYTES)||!write_at(raw,zeros,padding,end)||
    !write_at(raw,updated,sizeof updated,0)||ftruncate(raw,end+(off_t)padding)||fsync(raw))goto done;
 rc=0;
done:free(bytes);if(raw>=0&&close(raw))rc=2;if(preview>=0)close(preview);return rc;
}
