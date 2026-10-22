/* 单帧内存行段直接生成最终四分之一3FR；从不落盘原尺寸RAW。 */
#define _POSIX_C_SOURCE 200809L
#include "quarter_memory_file.h"
#include "factory_native_3fr.h"
#include "capture_preview_stream.h"
#include "photo_file_alignment.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
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
#define QM_W 5952u
#define QM_H 4421u
#define QM_BYTES (QM_W*QM_H*2u)
int qm_raw_header(const uint8_t *source,size_t preview,uint8_t *out){
 if(!source||!out||!preview||preview>16u*1024*1024||memcmp(source,"II*\0",4))return 0;
 uint8_t h[16384];memcpy(h,source,sizeof h);uint32_t main=f3_u32(h+4),raw,exif=0;
 if(!f3_scalar(h,sizeof h,main,330,&raw)||!f3_is(h,sizeof h,raw,256,11904)||
    !f3_is(h,sizeof h,raw,257,8842)||!f3_is(h,sizeof h,raw,258,16)||!f3_is(h,sizeof h,raw,259,1)||
    !f3_is(h,sizeof h,raw,273,16384)||!f3_is(h,sizeof h,raw,279,210510336)||
    !f3_put32(h,raw,256,QM_W)||!f3_put32(h,raw,257,QM_H)||!f3_put32(h,raw,278,QM_H)||
    !f3_put32(h,raw,279,QM_BYTES)||!f3_put_pair(h,raw,50719,62,46)||!f3_put_pair(h,raw,50720,5828,4371)||
    !f3_put32(h,main,273,16384+QM_BYTES)||!f3_put32(h,main,279,(uint32_t)preview))return 0;
 if(f3_tag(h,sizeof h,main,34665)&&!f3_scalar(h,sizeof h,main,34665,&exif))return 0;
 uint32_t dirs[3]={raw,main,exif};uint8_t *maker=NULL;
 for(unsigned i=0;i<3;i++)if(dirs[i]){
  uint8_t *t=f3_tag(h,sizeof h,dirs[i],37500);if(t){if(maker)return 0;maker=t;}
 }
 if(maker){uint8_t *t=f3_tag(h,sizeof h,f3_u32(maker+8),0x59);
  if(t){unsigned n=f3_u32(t+4);uint8_t *p=n>=5&&n<=32?f3_data(h,sizeof h,t,3,n,2):NULL;if(!p)return 0;
   f3_p16(p,1);f3_p16(p+2,62);f3_p16(p+4,46);f3_p16(p+6,5828);f3_p16(p+8,4371);
  }
 }
 f3_omit(h,raw,50830);f3_omit(h,main,305);memcpy(out,h,sizeof h);return 1;
}
static int qm_write(int fd,const void *v,size_t n){const uint8_t *p=v;while(n){ssize_t k=write(fd,p,n);if(k<0&&errno==EINTR)continue;if(k<=0||(size_t)k>n)return 0;p+=k;n-=(size_t)k;}return 1;}
int qm_raw_write(const char *path,const uint8_t *header,const RsPixelBlock *blocks,unsigned count,const uint8_t *preview,size_t bytes){
 uint8_t h[16384];size_t jpeg=capture_preview_stream_size(preview,bytes);unsigned end[RS_MAX_SEGMENTS],rows=0;
 if(!path||!blocks||!count||count>RS_MAX_SEGMENTS||!qm_raw_header(header,jpeg,h))return 0;
 for(unsigned i=0;i<count;i++){
  if(!blocks[i].pixels||((uintptr_t)blocks[i].pixels&1)||!blocks[i].bytes||blocks[i].bytes%(11904u*2)||blocks[i].bytes>210510336)return 0;
  rows+=(unsigned)(blocks[i].bytes/(11904u*2));end[i]=rows;
 }
 if(rows!=8842)return 0;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_BINARY,0600);if(fd<0)return 0;
 FactoryPrivateIdentity identity;int owned=factory_private_identity(fd,&identity),ok=owned&&qm_write(fd,h,sizeof h);
 uint16_t out[QM_W];
 for(unsigned y=0;ok&&y<QM_H;y++){
  unsigned sy=4*(y/2)+(y&1),by=sy+2<8842?sy+2:sy,ia=0,ib=0;
  while(sy>=end[ia])ia++;while(by>=end[ib])ib++;
  const uint16_t *a=blocks[ia].pixels+(size_t)(sy-(ia?end[ia-1]:0))*11904;
  const uint16_t *b=blocks[ib].pixels+(size_t)(by-(ib?end[ib-1]:0))*11904;
  for(unsigned x=0;x<QM_W;x++){unsigned sx=4*(x/2)+(x&1);out[x]=(uint16_t)(((uint32_t)a[sx]+a[sx+2]+b[sx]+b[sx+2]+2)/4);}
  ok=qm_write(fd,out,sizeof out);
 }
 const uint8_t zeros[4096]={0};size_t padding=photo_eof_padding(16384+QM_BYTES+jpeg);
 ok=ok&&qm_write(fd,preview,jpeg)&&qm_write(fd,zeros,padding)&&fsync(fd)==0;
 if(close(fd))ok=0;if(!ok&&owned)factory_private_remove(path,&identity);return ok;
}
