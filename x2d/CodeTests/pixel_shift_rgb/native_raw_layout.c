/* 合成 DNG 与原厂显影行对齐裸数据转换。逐行处理、独占输出、失败删除本次输出。 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "factory_native_3fr.h"
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
#define STRIDE 23424u
static FILE *output;
static const char *path;
static FactoryPrivateIdentity identity;
static int owned;
static _Noreturn void fail(const char *why){
 if(output)fclose(output);
 if(owned)factory_private_remove(path,&identity);
 fprintf(stderr,"NATIVE_RAW_LAYOUT_FAILED %s\n",why);exit(1);
}
static FILE *input(const char *name,long size){
 int fd=open(name,O_RDONLY|O_BINARY|O_NOFOLLOW);if(fd<0)fail("input");
 struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||(size>=0&&st.st_size!=size)){close(fd);fail("input size/type");}
 FILE *f=fdopen(fd,"rb");if(!f){close(fd);fail("input stream");}return f;
}
static int header_check(uint8_t *h){
 if(memcmp(h,"II\052\000",4))return 0;
 uint32_t ifd=f3_u32(h+4);
 const unsigned tags[]={256,257,258,259,262,277,273,279,50717};
 const uint32_t values[]={F3_WIDTH,F3_HEIGHT,16,1,32803,1,4096,F3_RAW_BYTES,65535};
 for(unsigned i=0;i<sizeof tags/sizeof tags[0];i++)if(!f3_is(h,4096,ifd,tags[i],values[i]))return 0;
 uint8_t *cfa=f3_data(h,4096,f3_tag(h,4096,ifd,33422),1,4,1);
 return cfa&&(!memcmp(cfa,"\000\001\001\002",4)||!memcmp(cfa,"\001\000\002\001",4));
}
static void native_header(FILE *source,uint8_t pattern[4]){
 uint8_t h[F3_HEADER];uint32_t raw,offset,bytes,preview,preview_size;
 if(fread(h,1,sizeof h,source)!=sizeof h||memcmp(h,"II\052\000",4))fail("native header");
 uint32_t first=f3_u32(h+4);
 if(!f3_scalar(h,sizeof h,first,330,&raw)||!f3_scalar(h,sizeof h,raw,273,&offset)||
    !f3_scalar(h,sizeof h,raw,279,&bytes)||offset!=F3_HEADER||bytes!=F3_RAW_BYTES||
    !f3_is(h,sizeof h,raw,256,F3_WIDTH)||!f3_is(h,sizeof h,raw,257,F3_HEIGHT)||
    !f3_is(h,sizeof h,raw,258,16)||!f3_is(h,sizeof h,raw,259,1)||
    !f3_is(h,sizeof h,raw,262,32803)||!f3_is(h,sizeof h,raw,277,1)||
    !f3_is(h,sizeof h,raw,278,F3_HEIGHT))fail("native raw contract");
 uint8_t *cfa=f3_data(h,sizeof h,f3_tag(h,sizeof h,raw,33422),1,4,1);
 if(!cfa||(memcmp(cfa,"\000\001\001\002",4)&&memcmp(cfa,"\001\000\002\001",4)))fail("native CFA");
 memcpy(pattern,cfa,4);
 if(!f3_scalar(h,sizeof h,first,273,&preview)||!f3_scalar(h,sizeof h,first,279,&preview_size)||
    preview!=F3_HEADER+F3_RAW_BYTES||preview_size<4||preview_size>16u*1024u*1024u)fail("native preview contract");
 struct stat st;if(fstat(fileno(source),&st)||st.st_size!=(int64_t)preview+preview_size)fail("native container length");
 if(fseek(source,(long)offset,SEEK_SET))fail("native raw seek");
}
static void packed_header(uint8_t h[4096],const uint8_t pattern[4]){
 const uint16_t tags[]={256,257,258,259,262,273,277,279,33422,50717};
 const uint32_t values[]={F3_WIDTH,F3_HEIGHT,16,1,32803,4096,1,F3_RAW_BYTES,0,65535};
 memset(h,0,4096);memcpy(h,"II\052\000",4);f3_p32(h+4,8);f3_p16(h+8,10);
 for(unsigned i=0;i<10;i++){
  uint8_t *p=h+10+12*i;f3_p16(p,tags[i]);f3_p16(p+2,i==8?1:4);f3_p32(p+4,i==8?4:1);
  if(i==8)memcpy(p+8,pattern,4);else f3_p32(p+8,values[i]);
 }
}
/* 与 3FR 头转换共用的静态函数在这个翻译单元不使用。 */
int main(int argc,char **argv){
 (void)factory_native_3fr_header;
 int reverse=argc==5&&!strcmp(argv[1],"padded-to-dng");
 int packed=argc==4&&!strcmp(argv[1],"3fr-to-packed");
 int native=argc==4&&(!strcmp(argv[1],"3fr-to-padded")||packed);
 if(!reverse&&!native&&(argc!=4||strcmp(argv[1],"dng-to-padded")))fail("arguments");
 path=argv[reverse?4:3];uint8_t h[4096];
 FILE *source=input(argv[2],native?-1:reverse?(long)STRIDE*F3_HEIGHT*2:4096L+F3_RAW_BYTES);
 if(native){uint8_t pattern[4];native_header(source,pattern);if(packed)packed_header(h,pattern);}
 if(reverse){FILE *metadata=input(argv[3],4096);if(fread(h,1,sizeof h,metadata)!=sizeof h)fail("header");fclose(metadata);}
 else if(!native&&fread(h,1,sizeof h,source)!=sizeof h)fail("header");
 if(!native&&!header_check(h))fail("header contract");
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_BINARY,0600);if(fd<0)fail("output collision");
 if(!factory_private_identity(fd,&identity)){close(fd);fail("output identity");}owned=1;
 output=fdopen(fd,"wb");if(!output){close(fd);fail("output stream");}
 if((reverse||packed)&&fwrite(h,1,sizeof h,output)!=sizeof h)fail("header write");
 uint16_t row[STRIDE];memset(row,0,sizeof row);
 for(unsigned y=0;y<F3_HEIGHT;y++){
  unsigned read_width=reverse?STRIDE:F3_WIDTH,write_width=(reverse||packed)?F3_WIDTH:STRIDE;
  if(fread(row,2,read_width,source)!=read_width)fail("row read");
  if(reverse)for(unsigned x=F3_WIDTH;x<STRIDE;x++)if(row[x])fail("nonzero row padding");
  if(fwrite(row,2,write_width,output)!=write_width)fail("row write");
 }
 if((!native&&fgetc(source)!=EOF)||ferror(source))fail("input tail");fclose(source);
 if(fflush(output)||fsync(fd))fail("sync");
 if(fclose(output)){output=NULL;fail("close");}output=NULL;
 puts("NATIVE_RAW_LAYOUT_COMPLETE_SAMPLES_PRESERVED");return 0;
}
