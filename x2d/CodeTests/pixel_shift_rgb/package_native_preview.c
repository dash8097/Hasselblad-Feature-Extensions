/* 原厂默认 FlashPix 预览流封装。仅文件处理，不触发拍摄或登记相册。
 * 主 JPEG 和预览由调用方提供；仅发布完整文件，拒绝覆盖。 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef PACKAGE_MOBILE_MPF
#include "compat_mobile_jpeg_exif.h"
#else
#include "factory_native_exif.h"
#endif
#include "photo_file_alignment.h"
#ifndef PACKAGE_IMAGE_WIDTH
#define PACKAGE_IMAGE_WIDTH 23326u
#define PACKAGE_IMAGE_HEIGHT 17498u
#endif
#ifndef _WIN32
#include <sys/syscall.h>
#endif
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
static FILE *dest;
static char *partial;
static int owned;
#ifndef _WIN32
static const char *staged_output;
static int staged_owned;
static struct stat staged_identity;
#endif
static const unsigned char magic[]={ 'F','P','X','R',0,0 };
static void fail(const char *reason) {
 if(dest)fclose(dest);
#ifndef _WIN32
 if(staged_owned){
  struct stat current;
  if(!lstat(staged_output,&current)&&current.st_dev==staged_identity.st_dev&&
     current.st_ino==staged_identity.st_ino)remove(staged_output);
 }
#endif
 if(owned)remove(partial);
 fprintf(stderr,"PREVIEW_PACKAGE_FAILED %s\n",reason);exit(1);
}
static unsigned be16(const unsigned char*p){return ((unsigned)p[0]<<8)|p[1];}
static void be32(unsigned char*p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void write_bytes(const void*p,size_t n){if(fwrite(p,1,n,dest)!=n)fail("write");}
static void app2(const unsigned char*p,unsigned n){
 if(n>65533)fail("marker length");
 unsigned char h[]={255,226,(unsigned char)((n+2)>>8),(unsigned char)(n+2)};
 write_bytes(h,4);write_bytes(p,n);
}
static unsigned utf16name(unsigned char*p,const char*s){
 unsigned n=0;while(*s){p[n++]=0;p[n++]=(unsigned char)*s++;}
 p[n++]=0;p[n++]=0;p[n++]=0;return n;
}
static void flashpix(const unsigned char *bytes,unsigned padded){
 unsigned char directory[99]={0};memcpy(directory,magic,6);directory[6]=1;directory[8]=2;
 memset(directory+9,255,4);unsigned at=13;at+=utf16name(directory+at,"/HASSELBLAD");
 static const unsigned char id[]={0x9b,0x5c,0x69,0x99,0x8f,0x83,0x6c,0x83,0x77,0x42,0x32,0x38,0x28,0x07,0x01,0x3f};
 memcpy(directory+at,id,16);at+=16;be32(directory+at,padded);at+=4;
 at+=utf16name(directory+at,"/HASSELBLAD/preview");if(at!=99)fail("directory contract");
 app2(directory,99);
 unsigned char fragment[65420];memcpy(fragment,magic,6);fragment[6]=2;fragment[7]=0;fragment[8]=1;
 for(unsigned offset=0;offset<padded;){
  unsigned n=padded-offset;if(n>65407)n=65407;
  be32(fragment+9,offset);memcpy(fragment+13,bytes+offset,n);app2(fragment,n+13);offset+=n;
 }
}
/* Validate SOF and APP headers without decoding or retaining the full main image. */
static long inspect(FILE*f,int preview,int adding_exif){
 unsigned char h[4],payload[65533];
 if(fread(h,1,2,f)!=2||h[0]!=255||h[1]!=216)fail("SOI");
 long insert=2;int leading=1,exif=0,sof=0;
 for(unsigned count=0;count<4096;count++){
  if(fread(h,1,4,f)!=4||h[0]!=255)fail("marker");
  unsigned kind=h[1],size=be16(h+2);
  if(size<2||fread(payload,1,size-2,f)!=size-2)fail("truncated marker");
  if(kind==226&&size>=8&&!memcmp(payload,magic,6))fail("existing FlashPix");
#ifdef PACKAGE_MOBILE_MPF
  if(kind==226&&size>=6&&!memcmp(payload,"MPF\0",4))fail("existing MPF");
#endif
  if(leading&&kind>=224&&kind<=239){
   if(kind==225&&size>=8&&!memcmp(payload,"Exif\0\0",6))exif=1;
   insert=ftell(f);if(insert<0)fail("tell");
  }else leading=0;
  if(kind==192){
   if(sof++||size!=17||payload[0]!=8||payload[5]!=3)fail("SOF baseline");
   unsigned height=be16(payload+1),width=be16(payload+3);
   if(width!=(preview?3888u:PACKAGE_IMAGE_WIDTH)||height!=(preview?2918u:PACKAGE_IMAGE_HEIGHT))fail("dimensions");
   if(preview&&(payload[7]!=0x21||payload[10]!=0x11||payload[13]!=0x11))fail("preview sampling");
  }else if(kind>=193&&kind<=207&&kind!=196&&kind!=200&&kind!=204)fail("unsupported SOF");
  if(kind==218){
   if(sof!=1||(!preview&&!adding_exif&&!exif))fail("missing baseline SOF or EXIF");
   if(!preview&&adding_exif&&exif)fail("capture EXIF would duplicate existing EXIF");
   return insert;
  }
 }
 fail("marker count");return -1;
}
static void copy(FILE*f,long n){
 unsigned char buf[65536];
 while(n){size_t want=n<0||n>(long)sizeof(buf)?sizeof(buf):(size_t)n;
  size_t got=fread(buf,1,want,f);if(!got){if(ferror(f)||n>0)fail("copy read");break;}
  write_bytes(buf,got);if(n>0)n-=(long)got;
 }
}
#ifndef _WIN32
/* exFAT 不接受 RENAME_NOREPLACE 时，仅允许在相册外的临时任务目录
 * 独占创建并复制。此文件在完成前不得被登记/消费；它不是原子发布接口。 */
static int staging_path(const char *path){
 const char *prefix="/mnt/media_rw/ssd/multishot/";
 size_t n=strlen(prefix);
 if(strncmp(path,prefix,n))return 0;
 const char *p=path+n;unsigned count=0;
 while(*p&&*p!='/'){
  if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'))return 0;
  ++p;if(++count>80)return 0;
 }
 if(!count||*p++!='/')return 0;
 count=0;
 while(*p){
  if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='.'))return 0;
  ++p;if(++count>80)return 0;
 }
 return count>4&&!strstr(path+n,"..");
}
static void exclusive_staged_copy(const char *output){
 FILE *input=fopen(partial,"rb");if(!input)fail("staged source open");
 int fd=open(output,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 if(fd<0){fclose(input);fail("staged output exists or inaccessible");}
 staged_output=output;
 if(fstat(fd,&staged_identity)){close(fd);fclose(input);fail("staged output identity");}
 staged_owned=1;dest=fdopen(fd,"wb");
 if(!dest){close(fd);fclose(input);fail("staged output stream");}
 copy(input,-1);fclose(input);
 if(fflush(dest)||fsync(fd))fail("staged flush");
 if(fclose(dest)){dest=NULL;fail("staged close");}dest=NULL;
 if(remove(partial))fail("staged partial cleanup");
 staged_owned=0;
}
#endif
int main(int argc,char**argv){
 int staged=0;const char *capture=NULL;
 if(argc<4)fail("expected main JPEG, default preview JPEG, output");
 for(int i=4;i<argc;i++){
  if(!strcmp(argv[i],"--staged-output")&&!staged)staged=1;
  else if(!strcmp(argv[i],"--direct-staged-output")&&!staged)staged=2;
  else if(!strcmp(argv[i],"--capture-raw")&&!capture&&i+1<argc)capture=argv[++i];
  else fail("unsupported or duplicate argument");
 }
#ifdef PACKAGE_MOBILE_MPF
 if(!capture)fail("mobile package requires same-capture RAW");
#endif
#ifndef _WIN32
 if(staged&&!staging_path(argv[3]))fail("staged output must be outside album in multishot task");
#else
 if(staged)fail("staged mode is device only");
#endif
 FILE*main=fopen(argv[1],"rb"),*preview=fopen(argv[2],"rb");
 if(!main||!preview)fail("input open");
 long insertion=inspect(main,0,capture!=NULL);inspect(preview,1,0);
 static uint8_t capture_header[16384],capture_exif[65536];uint32_t capture_exif_length=0;
 if(capture){
  FILE *f=fopen(capture,"rb");if(!f)fail("capture header open");
  size_t count=fread(capture_header,1,sizeof capture_header,f);int bad=ferror(f);fclose(f);
  if(bad||count!=sizeof capture_header||!factory_native_exif(capture_header,count,
     PACKAGE_IMAGE_WIDTH,PACKAGE_IMAGE_HEIGHT,capture_exif,sizeof capture_exif,&capture_exif_length))fail("capture metadata contract");
 }
 unsigned char ending[2];
 if(fseek(main,-2,SEEK_END)||fread(ending,1,2,main)!=2||ending[0]!=255||ending[1]!=217)fail("main EOI");
#ifdef PACKAGE_MOBILE_MPF
 long main_bytes=ftell(main);if(main_bytes<4)fail("main size");
#endif
 if(fseek(preview,0,SEEK_END))fail("preview seek");long size=ftell(preview);
 if(size<4||size>32*1024*1024||fseek(preview,0,SEEK_SET))fail("preview size");
 unsigned padded=((unsigned)size+4095)&~4095u;
 unsigned char*bytes=calloc(padded,1);if(!bytes)fail("preview allocation");
 if(fread(bytes,1,(size_t)size,preview)!=(size_t)size||bytes[size-2]!=255||bytes[size-1]!=217)fail("preview EOI");
 fclose(preview);
 partial=malloc(strlen(argv[3])+9);if(!partial)fail("path allocation");
 strcpy(partial,argv[3]);strcat(partial,".partial");
 /* 私有任务目录由调用方持锁：直接独占写临时成片，完成前不能发布或消费。
  * 只省去 exFAT 不支持原子排他 rename 时的第二次全文件复制。 */
 int fd=open(staged==2?argv[3]:partial,O_WRONLY|O_CREAT|O_EXCL|O_BINARY
#ifndef _WIN32
   |O_NOFOLLOW
#endif
   ,0600);if(fd<0)fail("partial exists or inaccessible");
#ifndef _WIN32
 if(staged==2){
  staged_output=argv[3];
  if(fstat(fd,&staged_identity)){close(fd);fail("staged identity");}
  staged_owned=1;
 }else
#endif
 owned=1;
 dest=fdopen(fd,"wb");if(!dest){close(fd);fail("output stream");}
 if(fseek(main,0,SEEK_SET))fail("main seek");
#ifdef PACKAGE_MOBILE_MPF
 /* 移植团队 X2D II 8645f06 的 MPF/COM 对齐布局，保持一代尺寸、
  * 原接口与排他发布。码流逐字节复制；MakerNote 仅保留同图机型标识。
  * 内层 SOI 占据 COM 的末两字节，内层 JPEG 起点位于 4KB 边界。 */
 (void)insertion;
 write_bytes("\xff\xd8",2);write_bytes(capture_exif,capture_exif_length);
 /* 一代 4.2.0 已验证的回放路径读取 FlashPix；保留它，同时登记 MPF。
  * 两套索引使用同一份首帧预览，不重新采样或编码。 */
 flashpix(bytes,padded);
 long prefix_end=ftell(dest);if(prefix_end<0)fail("header tell");
 uint64_t header_end=(uint64_t)prefix_end+90u;
 uint64_t main_start=(header_end+4u+4095u)&~UINT64_C(4095);
 uint64_t main_end=main_start+(uint64_t)main_bytes;
 uint64_t preview_start=(main_end+4095u)&~UINT64_C(4095);
 uint64_t mp_base=(uint64_t)prefix_end+8u;
 if(main_end>UINT32_MAX||preview_start>UINT32_MAX)fail("MPF offset overflow");
 unsigned char mp[90]={255,226,0,88,'M','P','F',0,'I','I',42,0};
 fep32(mp+12,8);fep16(mp+16,3);
 fep16(mp+18,0xb000);fep16(mp+20,7);fep32(mp+22,4);memcpy(mp+26,"0100",4);
 fep16(mp+30,0xb001);fep16(mp+32,4);fep32(mp+34,1);fep32(mp+38,2);
 fep16(mp+42,0xb002);fep16(mp+44,7);fep32(mp+46,32);fep32(mp+50,50);
 fep32(mp+58,0x30000);fep32(mp+62,(uint32_t)main_end);
 fep32(mp+74,0x10003);fep32(mp+78,(uint32_t)size);fep32(mp+82,(uint32_t)(preview_start-mp_base));
 write_bytes(mp,sizeof mp);
 unsigned com=(unsigned)(main_start-header_end);
 if(com<4||com>65535||(uint64_t)ftell(dest)!=header_end)fail("COM layout");
 unsigned char com_header[]={255,254,(unsigned char)(com>>8),(unsigned char)com};
 const unsigned char pad[4096]={0};
 write_bytes(com_header,sizeof com_header);write_bytes(pad,com-4);
 copy(main,-1);
 if((uint64_t)ftell(dest)!=main_end)fail("main extent");
 write_bytes(pad,(size_t)(preview_start-main_end));write_bytes(bytes,(size_t)size);
#else
 if(capture){copy(main,2);write_bytes(capture_exif,capture_exif_length);copy(main,insertion-2);}
 else copy(main,insertion);
 flashpix(bytes,padded);
 copy(main,-1);
#endif
 fclose(main);free(bytes);
 long end=ftell(dest);if(end<0)fail("output tell");
 const uint8_t zeros[4096]={0};write_bytes(zeros,photo_eof_padding((uint64_t)end));
 if(fflush(dest)||fsync(fd))fail("flush");
 if(fclose(dest)){dest=NULL;fail("close");}dest=NULL;
#ifndef _WIN32
 if(staged==2){
  staged_owned=0;free(partial);
  puts("STAGED_OUTPUT_COMPLETE_NOT_ALBUM_PUBLISHED");
  puts("NATIVE_PREVIEW_PACKAGE_COMPLETE");return 0;
 }
#endif
#ifdef _WIN32
 /* CRT rename refuses an existing destination on Windows. */
 if(rename(partial,argv[3]))fail("publish without overwrite");
#else
 /* 相机 SSD 为 exFAT，不支持硬链接。内核/文件系统不支持时拒绝发布，
  * 不能退化为可能覆盖用户成片的普通 rename。 */
 if(syscall(SYS_renameat2,AT_FDCWD,partial,AT_FDCWD,argv[3],1)){
  int error=errno;
  if(staged&&(error==EINVAL||error==ENOSYS||error==EOPNOTSUPP)){
   exclusive_staged_copy(argv[3]);
   puts("STAGED_OUTPUT_COMPLETE_NOT_ALBUM_PUBLISHED");
  }else{
   fprintf(stderr,"RENAME_NOREPLACE_ERRNO %d\n",error);
   fail("publish without overwrite");
  }
 }
#endif
 owned=0;free(partial);puts("NATIVE_PREVIEW_PACKAGE_COMPLETE");return 0;
}
