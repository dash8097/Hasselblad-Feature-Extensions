/* 仅相册外私有任务的 3FR 暂存文件。不是最终相册发布接口。 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "factory_native_3fr.h"
#include "factory_private_file.h"
#include "factory_overclock_model.h"
#include "photo_file_alignment.h"
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
static FILE *output;
static const char *destination;
static FactoryPrivateIdentity identity;
static int owned;
static double raw_clock(void){
 struct timespec t;
#ifdef _WIN32
 timespec_get(&t,TIME_UTC);
#else
 clock_gettime(CLOCK_MONOTONIC,&t);
#endif
 return t.tv_sec+t.tv_nsec*1e-9;
}
static void fail(const char *reason){
 if(output)fclose(output);
 if(owned)factory_private_remove(destination,&identity);
 fprintf(stderr,"NATIVE_RAW_PACKAGE_FAILED %s\n",reason);exit(1);
}
static FILE *source(const char *path,long expected){
 int fd=open(path,O_RDONLY|O_BINARY|O_NOFOLLOW);
 if(fd<0)fail("input open");struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||(expected>=0&&st.st_size!=expected)){close(fd);fail("input size or type");}
 FILE *f=fdopen(fd,"rb");if(!f){close(fd);fail("input stream");}return f;
}
static uint32_t preview_check(FILE *f){
 unsigned char b[4],payload[65533];unsigned sof=0;
 if(fread(b,1,2,f)!=2||b[0]!=255||b[1]!=216)fail("preview SOI");
 for(unsigned i=0;i<4096;i++){
  if(fread(b,1,4,f)!=4||b[0]!=255)fail("preview marker");
  unsigned marker=b[1],n=((unsigned)b[2]<<8)|b[3];
  if(n<2||fread(payload,1,n-2,f)!=n-2)fail("preview marker length");
  if(marker==192){
   if(sof++||n!=17||payload[0]!=8||payload[5]!=3||
     (((unsigned)payload[1]<<8)|payload[2])!=2918||
     (((unsigned)payload[3]<<8)|payload[4])!=3888||
     payload[7]!=0x21||payload[10]!=0x11||payload[13]!=0x11)fail("preview baseline geometry");
  }else if(marker>=193&&marker<=207&&marker!=196&&marker!=200&&marker!=204)fail("preview unsupported SOF");
  if(marker==218){
   if(sof!=1||fseek(f,-2,SEEK_END)||fread(b,1,2,f)!=2||b[0]!=255||b[1]!=217)fail("preview EOI");
   long size=ftell(f);if(size<4||size>16*1024*1024||fseek(f,0,SEEK_SET))fail("preview size");
   return (uint32_t)size;
  }
 }
 fail("preview marker count");return 0;
}
#ifndef _WIN32
static int private_path(const char *p){
 const char *prefix="/mnt/media_rw/ssd/multishot/";size_t n=strlen(prefix);
 if(strncmp(p,prefix,n))return 0;p+=n;unsigned count=0;
 while(*p&&*p!='/'){
  if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-')||++count>80)return 0;p++;
 }
 return count&&(!strcmp(p,"/final.3fr")||!strcmp(p,"/final.3FR"));
}
#endif
static void copy(FILE *f,uint32_t bytes){
 uint8_t buffer[65536];
 while(bytes){unsigned n=bytes>sizeof buffer?sizeof buffer:bytes;
  if(fread(buffer,1,n,f)!=n||fwrite(buffer,1,n,output)!=n)fail("copy");bytes-=n;
 }
 if(fgetc(f)!=EOF||ferror(f))fail("input tail");
}
int main(int argc,char **argv){
 double begin=raw_clock();
 if(argc!=6||((strcmp(argv[1],"4")&&strcmp(argv[1],"6"))))fail("arguments");
 destination=argv[5];
#ifndef _WIN32
 if(!private_path(destination))fail("private output path");
#endif
 uint8_t first[F3_HEADER],merged[4096],header[F3_HEADER];
 FILE *reference=source(argv[2],-1),*raw=source(argv[3],4096L+F3_RAW_BYTES),*preview=source(argv[4],-1);
 if(fread(first,1,sizeof first,reference)!=sizeof first||fread(merged,1,sizeof merged,raw)!=sizeof merged)fail("header read");
 fclose(reference);uint32_t preview_bytes=preview_check(preview);
 if(!factory_native_3fr_header(first,merged,argv[1][0]-'0',preview_bytes,header))fail("header contract");
 if(!factory_overclock_model(header,sizeof header))fail("model field contract");
 int fd=open(destination,O_WRONLY|O_CREAT|O_EXCL|O_BINARY|O_NOFOLLOW,0600);
 if(fd<0)fail("output exists or inaccessible");
 if(!factory_private_identity(fd,&identity)){close(fd);fail("output identity");}owned=1;
 output=fdopen(fd,"wb");if(!output){close(fd);fail("output stream");}
 if(fwrite(header,1,sizeof header,output)!=sizeof header)fail("header write");
 copy(raw,F3_RAW_BYTES);fclose(raw);copy(preview,preview_bytes);fclose(preview);
 const uint8_t zeros[4096]={0};
 size_t padding=photo_eof_padding(F3_HEADER+(uint64_t)F3_RAW_BYTES+preview_bytes);
 if(fwrite(zeros,1,padding,output)!=padding)fail("output padding");
 if(fflush(output)||fsync(fd))fail("output sync");
 if(fclose(output)){output=NULL;fail("output close");}output=NULL;
 printf("RAW_PACKAGE_MS %.0f\n",(raw_clock()-begin)*1000);
 puts("NATIVE_RAW_PRIVATE_PACKAGE_COMPLETE_NOT_ALBUM_PUBLISHED");return 0;
}
