/* X2D 4.2.0 实验：四张未压缩 3FR 逐行合成为插值 CFA DNG。
 * 不控制相机、不删除输入、不覆盖输出；不是原厂完整标定流程。 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#endif
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifdef FACTORY_PARALLEL_READ
#include "four_raw_reader.h"
#include "merge_workers.h"
#endif
#ifdef FACTORY_TEST_WIDTH
#define W FACTORY_TEST_WIDTH
#else
#define W 11663
#endif
#ifdef FACTORY_TEST_HEIGHT
#define H FACTORY_TEST_HEIGHT
#else
#define H 8749
#endif
#ifdef FACTORY_SIX
#ifndef FACTORY_NATIVE_DOMAIN
#error Six-shot synthesis requires preserved factory sample values
#endif
#define FRAME_COUNT 6
#else
#define FRAME_COUNT 4
#endif
static unsigned char headers[FRAME_COUNT][16384], output_header[4096];
static uint16_t input_row[11904], planes[2][W][4], output_rows[4*W];
static FILE *inputs[FRAME_COUNT];
#ifndef FACTORY_STDIO_ROWS
#define FACTORY_STDIO_ROWS 64u
#endif
static char input_buffers[FRAME_COUNT][FACTORY_STDIO_ROWS*23808],output_buffer[FACTORY_STDIO_ROWS*W*4];
static unsigned blacks[FRAME_COUNT],whites[FRAME_COUNT];
static int stream_mode;
/* 默认为原路径；预读和四线程合成均通过独立选项启用。 */
static int read_mode,compute_threads=1,background_write;
static double read_seconds,normalize_seconds,interpolate_seconds,write_seconds;
static double now(void) {
 struct timespec t;
#ifdef _WIN32
 timespec_get(&t,TIME_UTC);
#else
 clock_gettime(CLOCK_MONOTONIC,&t);
#endif
 return t.tv_sec+t.tv_nsec*1e-9;
}
static _Noreturn void fail(const char *m) { fprintf(stderr,"ERROR %s\n",m); exit(1); }
#ifdef FACTORY_PARALLEL_READ
#include "background_writer.h"
#endif
static int loopback(const char *name) {
#ifndef _WIN32
 char *end;long port=strtol(name,&end,10);if(*end||port<49152||port>65535)fail("loopback port");
 int s=socket(AF_INET,SOCK_STREAM,0);if(s<0)fail("loopback socket");
 struct timeval timeout={120,0};setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
 struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(0x7f000001);
 if(connect(s,(struct sockaddr *)&a,sizeof(a)))fail("loopback connect");return s;
#else
 (void)name;fail("loopback requires device build");return -1;
#endif
}
static unsigned u16(const unsigned char *p) { return p[0]|((unsigned)p[1]<<8); }
static uint32_t u32(const unsigned char *p) { return u16(p)|(u16(p+2)<<16); }
static void p16(unsigned char *p,unsigned v) {p[0]=v;p[1]=v>>8;}
static void p32(unsigned char *p,uint32_t v) {p16(p,v);p16(p+2,v>>16);}
static const unsigned char *tag(const unsigned char *b,unsigned ifd,unsigned wanted) {
 if(ifd>16382) fail("IFD bounds");
 unsigned n=u16(b+ifd); if(n>256 || ifd+2+12*n>16384) fail("IFD length");
 for(unsigned i=0;i<n;i++) {const unsigned char *t=b+ifd+2+12*i;if(u16(t)==wanted)return t;}
 fail("missing tag");return NULL;
}
static unsigned scalar(const unsigned char *b,unsigned ifd,unsigned wanted) {
 const unsigned char *t=tag(b,ifd,wanted);
 if(u32(t+4)!=1)fail("non scalar");
 if(u16(t+2)==3)return u16(t+8);
 if(u16(t+2)==4)return u32(t+8);
 fail("scalar type");return 0;
}
static const unsigned char *data(const unsigned char *b,unsigned ifd,unsigned wanted,unsigned type,unsigned count) {
 const unsigned char *t=tag(b,ifd,wanted);unsigned off=u32(t+8);
 if(u16(t+2)!=type||u32(t+4)!=count||off>16384-count*8)fail("rational metadata");
 return b+off;
}
static void validate(unsigned i,const char *name) {
 inputs[i]=stream_mode==3?fdopen(loopback(name),"rb"):fopen(name,"rb");if(!inputs[i])fail("input open");
 if(setvbuf(inputs[i],input_buffers[i],_IOFBF,sizeof(input_buffers[i])))fail("input buffering");
 unsigned char *b=headers[i];if(fread(b,1,16384,inputs[i])!=16384)fail("header read");
 if(memcmp(b,"II\052\000",4))fail("not little-endian TIFF");
 unsigned main=u32(b+4),sub=scalar(b,main,330),exif=scalar(b,main,34665);
 const unsigned char *maker=tag(b,exif,37500);
 const unsigned char *group=tag(b,u32(maker+8),33);
 unsigned off=u32(group+8);
 if(u16(group+2)!=9||u32(group+4)!=4||off>16368||u32(b+off)!=FRAME_COUNT||u32(b+off+4)!=i)fail("multishot sequence");
 if(scalar(b,sub,256)!=11904||scalar(b,sub,257)!=8842||scalar(b,sub,258)!=16||
    scalar(b,sub,259)!=1||scalar(b,sub,262)!=32803||scalar(b,sub,277)!=1||
    scalar(b,sub,273)!=16384||scalar(b,sub,279)!=210510336)fail("unsupported RAW layout");
 whites[i]=scalar(b,sub,50717);
 if(!whites[i]||whites[i]>65535||(i&&whites[i]!=whites[0]))fail("source white levels differ or invalid");
 const unsigned char *black=data(b,sub,50714,5,1);
 if(u32(black+4)!=1||u32(black)>8192)fail("black level");blacks[i]=u32(black);
 if(whites[i]<=blacks[i])fail("white below black level");
#ifdef FACTORY_NATIVE_DOMAIN
 if(i && blacks[i]!=blacks[0])fail("native-domain source black levels differ");
#endif
 const unsigned char *m=data(b,main,50721,10,9),*wb=data(b,main,50728,5,3);
#ifdef FACTORY_NATIVE_DISK_COLOR
 /* 原厂逐张写盘可能分别记录色彩矩阵/自动白平衡描述。
  * 六帧计算仍在未施加白平衡的传感器数值域中进行；输出头和原厂显影
  * 均沿用首帧。其他帧的有效色彩描述不应成为 RAW 合成的拒绝条件。
  * 仅原厂写盘构建启用，不放开组序号、布局、黑/白电平一致性检查。 */
 for(unsigned k=0;k<9;k++)if(!u32(m+8*k+4))fail("invalid color matrix denominator");
 for(unsigned k=0;k<3;k++)if(!u32(wb+8*k)||!u32(wb+8*k+4))fail("invalid white balance rational");
 if(i){
  int matrix_diff=memcmp(m,data(headers[0],u32(headers[0]+4),50721,10,9),72)!=0;
  int wb_diff=memcmp(wb,data(headers[0],u32(headers[0]+4),50728,5,3),24)!=0;
  if(matrix_diff||wb_diff)fprintf(stderr,"NATIVE_COLOR_REFERENCE_FIRST frame=%u matrix_diff=%d wb_diff=%d\n",i,matrix_diff,wb_diff);
 }
#else
 if(i && (memcmp(m,data(headers[0],u32(headers[0]+4),50721,10,9),72)||
           memcmp(wb,data(headers[0],u32(headers[0]+4),50728,5,3),24)))fail("color metadata differs");
#endif
}
static void row(unsigned y,uint16_t dst[W][4]) {
 static const int dx[]={0,0,-1,-1},dy[]={0,1,0,1};
 static const unsigned cfa[2][2]={{0,1},{3,2}};
 for(unsigned i=0;i<4;i++) {
  unsigned sy=y+1-dy[i],sx=-dx[i];
  double start=now();
  if(read_mode==2){
#ifdef FACTORY_PARALLEL_READ
   if(!four_reader_row(i,y,input_row))fail("parallel RAW row read");
#else
   fail("parallel reader not built");
#endif
  }else{
  if(stream_mode) {
   if(y==0)for(unsigned k=0;k<sy+92;k++)if(fread(input_row,2,11904,inputs[i])!=11904)fail("RAW leading rows");
  } else if((read_mode==0||y==0)&&fseek(inputs[i],16384L+(long)(sy+92)*23808L,SEEK_SET))fail("RAW seek");
  if(fread(input_row,2,11904,inputs[i])!=11904)fail("RAW row read");
  }
  read_seconds+=now()-start;start=now();
  for(unsigned x=0;x<W;x++) {
   unsigned c=cfa[sy%2][(sx+x)%2],v=input_row[124+sx+x];
#ifdef FACTORY_NATIVE_DOMAIN
   /* 原厂显影自行执行黑电平处理，合成前不扣黑、不裁剪暗部。 */
   dst[x][c]=(uint16_t)v;
#else
   double f=v>blacks[i]?(v-blacks[i])*65535.0/(65535-blacks[i]):0;
   dst[x][c]=(uint16_t)nearbyint(f);
#endif
  }
  normalize_seconds+=now()-start;
 }
}
#ifdef FACTORY_SIX
#include "onboard_six_rows.inc"
#endif
static unsigned entries=0,extra=512,entry_base=10;
static void entry(unsigned id,unsigned type,unsigned count,const void *buf,unsigned bytes) {
 unsigned char *t=output_header+entry_base+12*entries++;
 if(entries>30||extra+bytes>4096)fail("output header size");
 p16(t,id);p16(t+2,type);p32(t+4,count);
 if(bytes<=4)memcpy(t+8,buf,bytes);
 else {p32(t+8,extra);memcpy(output_header+extra,buf,bytes);extra=(extra+bytes+3)&~3U;}
}
static void number(unsigned id,unsigned type,unsigned n) {unsigned char b[4];p32(b,n);entry(id,type,1,b,type==3?2:4);}
static void copy_optional(unsigned ifd,unsigned wanted) {
 const unsigned char *b=headers[0];unsigned n=u16(b+ifd);
 if(ifd+2+12*n>16384)fail("metadata IFD bounds");
 for(unsigned i=0;i<n;i++) {
  const unsigned char *t=b+ifd+2+12*i;
  if(u16(t)!=wanted)continue;
  unsigned type=u16(t+2),count=u32(t+4),size;
  switch(type){case 1:case 2:case 7:size=1;break;case 3:size=2;break;case 4:case 9:size=4;break;case 5:case 10:size=8;break;default:fail("metadata type");return;}
  if(count>128)fail("metadata length");unsigned bytes=size*count;
  unsigned off=u32(t+8);if(bytes>4 && (off>16384 || bytes>16384-off))fail("metadata data bounds");
  entry(wanted,type,count,bytes<=4?t+8:b+off,bytes);return;
 }
}
static void make_header(void) {
 memcpy(output_header,"II\052\000",4);p32(output_header+4,8);
 number(256,4,W*2);number(257,4,H*2);number(258,3,16);number(259,3,1);number(262,3,32803);
 unsigned main=u32(headers[0]+4);
 copy_optional(main,271);copy_optional(main,272);
 number(273,4,4096);number(274,3,1);number(277,3,1);number(278,4,H*2);number(279,4,W*H*8);number(284,3,1);
 copy_optional(main,306);
 const unsigned char dim[]={2,0,2,0},version[]={1,4,0,0},back[]={1,1,0,0},colors[]={0,1,2};
#ifdef FACTORY_SIX
 const unsigned char cfa[]={1,0,2,1};
#else
 const unsigned char cfa[]={0,1,1,2};
#endif
 entry(33421,3,2,dim,4);entry(33422,1,4,cfa,4);entry(50706,1,4,version,4);entry(50707,1,4,back,4);
 const char model[]="Hasselblad X2D 100C";
 entry(50708,2,sizeof(model),model,sizeof(model));entry(50710,1,3,colors,3);number(50711,3,1);
#ifdef FACTORY_NATIVE_DOMAIN
 number(50714,4,blacks[0]);
#else
 number(50714,4,0);
#endif
#ifdef FACTORY_NATIVE_DOMAIN
 number(50717,4,whites[0]);
#else
 number(50717,4,65535);
#endif
 entry(50721,10,9,data(headers[0],main,50721,10,9),72);
 entry(50728,5,3,data(headers[0],main,50728,5,3),24);number(50778,3,0);
 number(34665,4,2048);
 p16(output_header+8,entries);
 /* TIFF 要求目录按 tag 递增；EXIF 指针是在后面补入。 */
 for(unsigned i=0;i<entries;i++)for(unsigned j=i+1;j<entries;j++)if(u16(output_header+10+i*12)>u16(output_header+10+j*12)){
  unsigned char tmp[12];memcpy(tmp,output_header+10+i*12,12);memcpy(output_header+10+i*12,output_header+10+j*12,12);memcpy(output_header+10+j*12,tmp,12);
 }
 if(extra>=2048)fail("main metadata overlap");
 entries=0;entry_base=2050;extra=2560;
 unsigned exif=scalar(headers[0],main,34665);
 const unsigned keep[]={33434,33437,34850,34855,36864,36867,36868,37377,37378,37380,37386,40960,40961,41986,41987,42034,42036};
 for(unsigned i=0;i<sizeof(keep)/sizeof(keep[0]);i++)copy_optional(exif,keep[i]);
 p16(output_header+2048,entries);
}
#ifndef FACTORY_SIX
struct FourCompute {uint16_t (*a)[4],(*b)[4];};
static void four_compute(unsigned begin,unsigned end,void *opaque){
 struct FourCompute *c=opaque;uint16_t (*a)[4]=c->a,(*b)[4]=c->b;
 for(unsigned x=begin;x<end;x++) {
   unsigned xn=x+1<W?x+1:x;
   output_rows[2*x]=a[x][0];output_rows[2*x+1]=(a[x][1]+(unsigned)a[xn][1]+1)/2;
   output_rows[2*W+2*x]=(a[x][3]+(unsigned)b[x][3]+1)/2;
   output_rows[2*W+2*x+1]=(a[x][2]+(unsigned)b[x][2]+a[xn][2]+b[xn][2]+2)/4;
  }
}
#endif
int main(int argc,char **argv) {
 double start_total=now();
 if(argc<FRAME_COUNT+2)fail("expected output and ordered RAW inputs");
 for(int arg=FRAME_COUNT+2;arg<argc;arg++){
  if(!strcmp(argv[arg],"--sequential-read")&&!read_mode)read_mode=1;
  else if(!strcmp(argv[arg],"--parallel-read")&&!read_mode){
#ifdef FACTORY_PARALLEL_READ
   read_mode=2;
#else
   fail("parallel reader not built");
#endif
  }else if(!strcmp(argv[arg],"--compute-threads=4")&&compute_threads==1){
#ifdef FACTORY_PARALLEL_READ
   compute_threads=4;
#else
   fail("parallel compute not built");
#endif
  }else if(!strcmp(argv[arg],"--background-write")&&!background_write){
#ifdef FACTORY_PARALLEL_READ
   background_write=1;
#else
   fail("background writer not built");
#endif
  }else if(!strcmp(argv[arg],"--stream")&&!stream_mode)stream_mode=1;
  else if(!strcmp(argv[arg],"--loopback")&&!stream_mode)stream_mode=3;
  else fail("unsupported or repeated option");
 }
 if(stream_mode&&(read_mode||compute_threads!=1||background_write||FRAME_COUNT!=4))fail("stream option combination");
 for(unsigned i=0;i<FRAME_COUNT;i++)validate(i,argv[i+2]);
 make_header();
 int fd=stream_mode==3?loopback(argv[1]):open(argv[1],stream_mode?O_WRONLY|O_BINARY:O_WRONLY|O_CREAT|O_EXCL|O_BINARY,0600);if(fd<0)fail("output exists or open failed");
#ifndef _WIN32
 if(stream_mode==1){struct stat st;if(fstat(fd,&st)||!S_ISFIFO(st.st_mode))fail("stream output must be FIFO");}
#endif
 FILE *out=fdopen(fd,"wb");if(!out)fail("output stream");
 if(setvbuf(out,output_buffer,_IOFBF,sizeof(output_buffer)))fail("output buffering");
 if(fwrite(output_header,1,4096,out)!=4096)fail("header write");
#ifdef FACTORY_PARALLEL_READ
 if(read_mode==2){
  if(atexit(four_readers_stop)||!four_readers_start(inputs,H,FRAME_COUNT))fail("parallel reader start");
 }
#endif
#ifdef FACTORY_PARALLEL_READ
 if(compute_threads==4&&(atexit(merge_workers_stop)||!merge_workers_start()))fail("compute worker start");
 if(background_write&&(atexit(output_writer_stop)||!output_writer_start(out,4*W)))fail("background writer start");
#endif
 row(0,planes[0]);
 for(unsigned y=0;y<H;y++) {
  uint16_t (*a)[4]=planes[y%2],(*b)[4]=planes[(y+1)%2];
  if(y+1<H)row(y+1,b);else memcpy(b,a,sizeof(planes[0]));
#ifdef FACTORY_SIX
  /* 第五、六张读取和整理不计入纯插值计算。 */
  six_prepare_rows(y);
#endif
  double start=now();
#ifdef FACTORY_SIX
  six_output_rows(y,a,b);
#else
  struct FourCompute task={a,b};
#ifdef FACTORY_PARALLEL_READ
  if(compute_threads==4)merge_workers_run(W,four_compute,&task);else
#endif
  four_compute(0,W,&task);
#endif
  interpolate_seconds+=now()-start;start=now();
  if(background_write){
#ifdef FACTORY_PARALLEL_READ
   if(!output_writer_put(output_rows))fail("background image write");
#endif
  }else if(fwrite(output_rows,2,4*W,out)!=4*W)fail("image write");
  write_seconds+=now()-start;
  if(y%1000==0){printf("ROW %u/%u\n",y,H);fflush(stdout);}
 }
 double flush_start=now();
#ifdef FACTORY_PARALLEL_READ
 if(background_write&&!output_writer_finish())fail("background writer finish");
#endif
 if(fflush(out)||(!stream_mode && fsync(fd))||fclose(out))fail("output flush");
 double flush_seconds=now()-flush_start;
#ifdef FACTORY_PARALLEL_READ
 if(compute_threads==4)merge_workers_stop();
 if(read_mode==2)four_readers_stop();
#endif
 for(unsigned i=0;i<FRAME_COUNT;i++)fclose(inputs[i]);
 printf("TIMING_SECONDS total=%.3f read=%.3f normalize=%.3f interpolate=%.3f write=%.3f flush=%.3f\n",now()-start_total,read_seconds,normalize_seconds,interpolate_seconds,write_seconds,flush_seconds);
 printf("MERGE_TOTAL_MS %.0f\nMERGE_READ_WAIT_MS %.0f\nMERGE_NORMALIZE_MS %.0f\nMERGE_COMPUTE_MS %.0f\nMERGE_WRITE_WAIT_MS %.0f\nMERGE_FLUSH_MS %.0f\n",(now()-start_total)*1000,read_seconds*1000,normalize_seconds*1000,interpolate_seconds*1000,write_seconds*1000,flush_seconds*1000);
#ifdef FACTORY_PARALLEL_READ
 if(read_mode==2){
  unsigned calls=0;for(unsigned i=0;i<FRAME_COUNT;i++)calls+=four_readers[i].read_calls;
  printf("MERGE_READER_BATCH_ROWS %u\nMERGE_READER_THREADS %u\nMERGE_READER_CALLS %u\n",reader_batch_rows,FRAME_COUNT,calls);
 }
 if(background_write)printf("BACKGROUND_WRITE_SECONDS io=%.3f queue_wait_and_copy=%.3f drain_and_flush=%.3f\n",output_writer.io_seconds,write_seconds,flush_seconds);
 if(background_write)printf("MERGE_WRITER_IO_MS %.0f\n",output_writer.io_seconds*1000);
#endif
 puts(FRAME_COUNT==6?"ONBOARD_SIX_COMPLETE":"ONBOARD_FOUR_COMPLETE");return 0;
}
