#define _POSIX_C_SOURCE 200809L
#include "raw_stack_file.h"
#include "factory_native_3fr.h"
#include "capture_preview_stream.h"
#include "photo_file_alignment.h"
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "factory_private_file.h"
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
int rs_raw_header(const uint8_t source[16384],size_t preview_bytes,uint8_t out[16384]){
 if(!source||!out||!preview_bytes||preview_bytes>16u*1024*1024||memcmp(source,"II*\0",4))return 0;
 uint8_t header[16384];memcpy(header,source,sizeof header);
 uint32_t main=f3_u32(header+4),raw;
 F3Occupied occupied={0};memset(occupied.used,1,8);
 if(!f3_mark_ifd(header,main,&occupied)||!f3_scalar(header,sizeof header,main,330,&raw)||
    !f3_is(header,sizeof header,raw,256,11904)||!f3_is(header,sizeof header,raw,257,8842)||
    !f3_is(header,sizeof header,raw,258,16)||!f3_is(header,sizeof header,raw,259,1)||
    !f3_is(header,sizeof header,raw,273,16384)||!f3_is(header,sizeof header,raw,279,210510336)||
    !f3_put32(header,main,273,16384+210510336)||!f3_put32(header,main,279,(uint32_t)preview_bytes))return 0;
 /* RAW 宽高、裁切、CFA、黑电平、镜头标定、曝光信息保持普通单帧布局。 */
 memcpy(out,header,sizeof header);return 1;
}
static int write_all(int fd,const void *source,size_t bytes){
 const uint8_t *p=source;
 while(bytes){size_t block=bytes>1048576?1048576:bytes;ssize_t n=write(fd,p,block);
  if(n<0&&errno==EINTR)continue;if(n<=0||(size_t)n>block)return 0;
  p+=n;bytes-=(size_t)n;
 }
 return 1;
}
int rs_raw_write(const char *path,const uint8_t source[16384],const uint16_t *pixels,size_t bytes,
 const uint8_t *preview,size_t preview_bytes){
 RsPixelBlock block={pixels,bytes};return rs_raw_write_blocks(path,source,&block,1,preview,preview_bytes);
}
int rs_raw_write_blocks(const char *path,const uint8_t source[16384],const RsPixelBlock *blocks,unsigned count,
 const uint8_t *preview,size_t preview_bytes){
 uint8_t header[16384];size_t jpeg=capture_preview_stream_size(preview,preview_bytes);
 if(!path||!blocks||!count||count>RS_MAX_SEGMENTS||!jpeg||!rs_raw_header(source,jpeg,header))return 0;
 size_t bytes=0;
 for(unsigned i=0;i<count;i++){
  if(!blocks[i].pixels||!blocks[i].bytes||(blocks[i].bytes&1)||blocks[i].bytes>210510336-bytes)return 0;
  bytes+=blocks[i].bytes;
 }
 if(bytes!=210510336)return 0;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_BINARY,0600);if(fd<0)return 0;
 FactoryPrivateIdentity created;struct stat current;int owned=factory_private_identity(fd,&created),ok=owned;
 ok=ok&&write_all(fd,header,sizeof header);
 for(unsigned i=0;ok&&i<count;i++)ok=write_all(fd,blocks[i].pixels,blocks[i].bytes);
 size_t total=sizeof header+bytes+jpeg,padding=photo_eof_padding(total);
 const uint8_t zeros[4096]={0};
 ok=ok&&write_all(fd,preview,jpeg)&&write_all(fd,zeros,padding)&&fsync(fd)==0;
 ok=ok&&fstat(fd,&current)==0&&current.st_size==(off_t)(total+padding);
 if(close(fd))ok=0;
 if(!ok&&owned)factory_private_remove(path,&created);
 return ok;
}
