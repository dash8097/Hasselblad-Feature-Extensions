/* 官方4.2.0普通单张3FR的四分之一候选：同色四合一，保留原厂封装。
 * 仅处理调用者提供的私有暂存文件，不访问相机或相册。 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "factory_native_3fr.h"
#include "factory_private_file.h"
#include "quarter_single_core.h"
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
#define QW 5952u
#define QH 4421u
#define QB (QW*QH*2u)
static const char *dest;
static FactoryPrivateIdentity identity;
static FILE *output;
static int owned;
static __attribute__((noreturn)) void fail(const char *reason){
 if(output)fclose(output);
 if(owned)factory_private_remove(dest,&identity);
 fprintf(stderr,"QUARTER_NATIVE_RAW_FAILED %s\n",reason);exit(1);
}
static void resize_header(uint8_t *h,uint32_t preview_size){
 uint32_t main=f3_u32(h+4),raw,exif=0;
 F3Occupied used={0};memset(used.used,1,8);
 if(memcmp(h,"II*\0",4)||!f3_mark_ifd(h,main,&used)||!f3_scalar(h,F3_HEADER,main,330,&raw))fail("directories");
 const unsigned tags[]={256,257,258,259,262,273,277,279};
 const uint32_t values[]={11904,8842,16,1,32803,16384,1,210510336};
 for(unsigned i=0;i<8;i++)if(!f3_is(h,F3_HEADER,raw,tags[i],values[i]))fail("source geometry");
 if(!f3_put32(h,raw,256,QW)||!f3_put32(h,raw,257,QH)||!f3_put32(h,raw,278,QH)||
    !f3_put32(h,raw,279,QB)||!f3_put_pair(h,raw,50719,62,46)||!f3_put_pair(h,raw,50720,5828,4371)||
    !f3_put32(h,main,273,F3_HEADER+QB)||!f3_put32(h,main,279,preview_size))fail("output geometry");
 if(f3_tag(h,F3_HEADER,main,34665)&&!f3_scalar(h,F3_HEADER,main,34665,&exif))fail("exif");
 uint8_t *maker=NULL;uint32_t dirs[3]={raw,main,exif};
 for(unsigned i=0;i<3;i++)if(dirs[i]){
  uint8_t *tag=f3_tag(h,F3_HEADER,dirs[i],37500);
  if(tag){if(maker)fail("ambiguous maker note");maker=tag;}
 }
 if(maker){
  uint8_t *tag=f3_tag(h,F3_HEADER,f3_u32(maker+8),0x59);
  if(tag){unsigned count=f3_u32(tag+4);uint8_t *crop=count>=5&&count<=32?f3_data(h,F3_HEADER,tag,3,count,2):NULL;
   if(!crop)fail("maker crop");
   f3_p16(crop,1);f3_p16(crop+2,62);f3_p16(crop+4,46);f3_p16(crop+6,5828);f3_p16(crop+8,4371);
  }
 }
 /* 原厂同样在位移RAW封装中移除这一空间表；不能沿用未缩放的表格坐标。 */
 f3_omit(h,raw,50830);f3_omit(h,main,305);
}
int main(int argc,char **argv){
 if(argc!=3)fail("usage: quarter-native-raw source.3FR result.3FR");dest=argv[2];
 int fd=open(argv[1],O_RDONLY|O_NOFOLLOW|O_BINARY);struct stat st;
 if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<210526720)fail("source type or length");
 FILE *input=fdopen(fd,"rb");if(!input)fail("input stream");uint8_t header[F3_HEADER];
 if(fread(header,1,sizeof header,input)!=sizeof header)fail("source header");
 uint32_t main=f3_u32(header+4),preview_offset,preview_size;
 if(!f3_scalar(header,F3_HEADER,main,273,&preview_offset)||!f3_scalar(header,F3_HEADER,main,279,&preview_size)||
    !preview_size||preview_size>16u*1024*1024||preview_offset<210526720||
    (uint64_t)preview_offset+preview_size>(uint64_t)st.st_size)fail("preview range");
 resize_header(header,preview_size);
 fd=open(dest,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_BINARY,0600);if(fd<0)fail("existing output");
 if(!factory_private_identity(fd,&identity)){close(fd);fail("output identity");}owned=1;
 output=fdopen(fd,"wb");if(!output){close(fd);fail("output stream");}
 if(fwrite(header,1,sizeof header,output)!=sizeof header||fseek(input,F3_HEADER,SEEK_SET))fail("header write");
 uint16_t in[4*11904],out[2*QW];
 for(unsigned y=0;y<QH;y+=2){unsigned rows=QH-y>=2?2:1;
  if(fread(in,2,2*rows*11904,input)!=2*rows*11904||
     !quarter_raw_rows(in,sizeof in,11904,2*rows,11904,0,0,QW,rows,out,sizeof out,QW,0,rows)||
     fwrite(out,2,rows*QW,output)!=rows*QW)fail("raw rows");
 }
 if(fseek(input,(long)preview_offset,SEEK_SET))fail("preview seek");uint8_t block[65536];unsigned left=preview_size;
 while(left){unsigned n=left>sizeof block?sizeof block:left;
  if(fread(block,1,n,input)!=n||fwrite(block,1,n,output)!=n)fail("preview copy");left-=n;
 }
 long length=ftell(output);if(length<0)fail("length");uint8_t zero[4096]={0};unsigned tail=(4096-(unsigned long)length%4096)%4096;
 if(fwrite(zero,1,tail,output)!=tail||fflush(output)||fsync(fileno(output)))fail("durable output");
 if(fclose(output)){output=NULL;fail("close");}output=NULL;fclose(input);
 puts("QUARTER_NATIVE_RAW_DURABLE_4K_EFFECTIVE_5828_4371_SOURCE_RETAINED");return 0;
}
