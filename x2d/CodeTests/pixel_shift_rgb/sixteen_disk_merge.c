/* 十六帧磁盘合成候选。四线程各独占四个源文件，分块重建半像素 RGBG，
 * 再采样为十六亿 RGGB。只检查头部、读写结果与范围，不逐像素校验。
 * 不控制相机，不删除输入，不登记相册；输出失败只移除自己新建的文件。 */
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "sixteen_disk_policy.h"
#ifndef SIXTEEN_WIDTH
#define SIXTEEN_WIDTH 11663u
#endif
#ifndef SIXTEEN_HEIGHT
#define SIXTEEN_HEIGHT 8749u
#endif
#define F3_WIDTH (4u*SIXTEEN_WIDTH)
#define F3_HEIGHT (4u*SIXTEEN_HEIGHT)
#define CAPTURE_MERGED_RGGB 1
#include "capture_merged_header.h"
/* 共用经过测试的跨平台线程原语。六路预读结构不分配也不启动。 */
#include "four_raw_reader.h"
#include "merge_workers.h"
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#undef fseeko
#undef ftello
#define fseeko _fseeki64
#define ftello _ftelli64
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
typedef struct {
 FILE *files[16];
 uint16_t *scratch[4],*planes,*rows;
 uint8_t headers[16][F3_HEADER],final_header[F3_HEADER];
 unsigned top,count,read_count,batch,plane_width,plane_rows,write_y,write_count;
 int errors[4];
 uint64_t read_calls[4];
 uint32_t preview_offset,preview_bytes;
} SixteenMerge;
static SixteenMerge m;
static int source_rejected(unsigned frame,const char *field){
 fprintf(stderr,"SIXTEEN_SOURCE_REJECTED frame=%u field=%s\n",frame,field);return 0;
}
static int seek_at(FILE *f,uint64_t offset){return fseeko(f,(int64_t)offset,SEEK_SET)==0;}
static FILE *open_source(const char *path){
 int fd=open(path,O_RDONLY|O_BINARY|O_NOFOLLOW);if(fd<0)return NULL;
 struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);return NULL;}
 FILE *f=fdopen(fd,"rb");if(!f)close(fd);return f;
}
static int validate_source(unsigned index,const char *path){
 FILE *f=open_source(path);if(!f)return source_rejected(index,"open");m.files[index]=f;
 uint8_t *b=m.headers[index];uint32_t main,raw,exif;
 if(fread(b,1,F3_HEADER,f)!=F3_HEADER||memcmp(b,"II*\0",4))return source_rejected(index,"header");
 main=f3_u32(b+4);
 if(!f3_scalar(b,F3_HEADER,main,330,&raw))return source_rejected(index,"raw_ifd");
 const unsigned tags[]={256,257,258,259,262,277,273,279};
 const uint32_t values[]={11904,8842,16,1,32803,1,16384,210510336};
 for(unsigned i=0;i<8;i++)if(!f3_is(b,F3_HEADER,raw,tags[i],values[i]))return source_rejected(index,"raw_layout");
 uint8_t *maker=f3_tag(b,F3_HEADER,raw,37500);
 if(!maker)maker=f3_tag(b,F3_HEADER,main,37500);
 if(!maker&&f3_scalar(b,F3_HEADER,main,34665,&exif))maker=f3_tag(b,F3_HEADER,exif,37500);
 uint8_t *group=maker?f3_data(b,F3_HEADER,f3_tag(b,F3_HEADER,f3_u32(maker+8),33),9,4,4):NULL;
 if(!group||f3_u32(group)!=16||f3_u32(group+4)!=index)return source_rejected(index,"group");
 uint8_t *black=f3_data(b,F3_HEADER,f3_tag(b,F3_HEADER,raw,50714),5,1,8);
 uint32_t white;
 if(!black||f3_u32(black+4)!=1||f3_u32(black)>8192||
    !f3_scalar(b,F3_HEADER,raw,50717,&white)||white<=f3_u32(black)||white>65535)return source_rejected(index,"levels");
 uint8_t *matrix=f3_data(b,F3_HEADER,f3_tag(b,F3_HEADER,main,50721),10,9,8);
 uint8_t *balance=f3_data(b,F3_HEADER,f3_tag(b,F3_HEADER,main,50728),5,3,8);
 if(!matrix||!balance)return source_rejected(index,"color_tags");
 for(unsigned k=0;k<9;k++)if(!f3_u32(matrix+8*k+4))return source_rejected(index,"matrix_denominator");
 for(unsigned k=0;k<3;k++)if(!f3_u32(balance+8*k)||!f3_u32(balance+8*k+4))
  return source_rejected(index,"white_balance_rational");
 if(index){
  uint32_t first=f3_u32(m.headers[0]+4),first_raw,first_white;
  if(!f3_scalar(m.headers[0],F3_HEADER,first,330,&first_raw)||
     !f3_scalar(m.headers[0],F3_HEADER,first_raw,50717,&first_white)||first_white!=white)return source_rejected(index,"white_level_mismatch");
  uint8_t *first_black=f3_data(m.headers[0],F3_HEADER,f3_tag(m.headers[0],F3_HEADER,first_raw,50714),5,1,8);
  if(!first_black||memcmp(black,first_black,8))return source_rejected(index,"black_level_mismatch");
  uint8_t *first_matrix=f3_data(m.headers[0],F3_HEADER,f3_tag(m.headers[0],F3_HEADER,first,50721),10,9,8);
  uint8_t *first_balance=f3_data(m.headers[0],F3_HEADER,f3_tag(m.headers[0],F3_HEADER,first,50728),5,3,8);
  if(!first_matrix||memcmp(matrix,first_matrix,72))return source_rejected(index,"matrix_mismatch");
  if(!first_balance)return source_rejected(index,"first_white_balance");
  /* Bayer 合成未施加白平衡；首帧头部及 rawpb 决定整张成片的白平衡。
   * 原厂逐帧 AWB 可以变化，不能用字节相等作为同一组的条件。 */
  if(memcmp(balance,first_balance,24))fprintf(stderr,"SIXTEEN_WB_REFERENCE_FIRST frame=%u\n",index);
 }
 if(fseeko(f,0,SEEK_END))return source_rejected(index,"seek_end");
 int64_t bytes=ftello(f);if(bytes<(int64_t)F3_HEADER+210510336)return source_rejected(index,"source_length");
 if(!index){
  if(!f3_is(b,F3_HEADER,main,259,7)||!f3_scalar(b,F3_HEADER,main,273,&m.preview_offset)||
     !f3_scalar(b,F3_HEADER,main,279,&m.preview_bytes)||m.preview_bytes<4||
     m.preview_bytes>16u*1024*1024||m.preview_offset<F3_HEADER+210510336u||
     (uint64_t)m.preview_offset+m.preview_bytes>(uint64_t)bytes)return source_rejected(index,"preview_layout");
 }
 return 1;
}
static int read_source_rows(FILE *file,uint16_t *rows,unsigned first,unsigned count){
 if(!count||first>8842||count>1025)return 0;
 unsigned available=first<8842?8842-first:0;
 if(available>count)available=count;
 if(available&&(!seek_at(file,16384+(uint64_t)first*23808)||
    fread(rows,2,(size_t)available*11904,file)!=(size_t)available*11904))return 0;
 /* 位移后的底边只扩展同CFA奇偶性的最近有效行，不读取预览区。
  * 主体仍一次连续读块；只有边缘行需要额外读取或行复制。 */
 for(unsigned y=available;y<count;y++){
  unsigned source=first+y;
  while(source>=8842)source-=2;
  if(source>=first&&source<first+available){
   memcpy(rows+(size_t)y*11904,rows+(size_t)(source-first)*11904,23808);
  }else if(!seek_at(file,16384+(uint64_t)source*23808)||
     fread(rows+(size_t)y*11904,2,11904,file)!=11904)return 0;
 }
 return 1;
}
static void load_range(unsigned begin,unsigned end,void *opaque){
 SixteenMerge *s=opaque;unsigned id=begin/4;
 if(end-begin!=4||id>3){s->errors[0]=1;return;}
 static const unsigned cfa[2][2]={{0,1},{3,2}};
 for(unsigned i=begin;i<end;i++){
  unsigned dx=x2d_sixteen_phase[i][0],dy=x2d_sixteen_phase[i][1];
  unsigned sx=(dx+1)/2,sy=(4-dy)/2,hx=2*sx-dx,hy=2*sy+dy-3;
#ifdef SIXTEEN_TEST_READ_FAILURE
  if(i==7&&s->top>=2){s->errors[id]=1;return;}
#endif
  if(!read_source_rows(s->files[i],s->scratch[id],92+sy+s->top,s->read_count)){
   s->errors[id]=1;return;
  }
  s->read_calls[id]++;
  for(unsigned y=0;y<s->read_count;y++){
   const uint16_t *raw=s->scratch[id]+(size_t)y*11904+124+sx;
   uint16_t *dst=s->planes+((size_t)(hy+2*y)*s->plane_width+hx)*4;
   for(unsigned x=0;x<SIXTEEN_WIDTH;x++){
    unsigned color=cfa[(sy+s->top+y)&1][(sx+x)&1];
    dst[8*x+color]=raw[x];
   }
  }
 }
}
static void remosaic_range(unsigned begin,unsigned end,void *opaque){
 SixteenMerge *s=opaque;unsigned pw=s->plane_width;
 for(unsigned i=begin;i<end;i++){
  unsigned y=s->write_y+i;
  unsigned yn=(2*s->top+y+1<2*SIXTEEN_HEIGHT)?y+1:y;
  uint16_t *out=s->rows+(size_t)i*2*F3_WIDTH;
  const uint16_t *a=s->planes+(size_t)y*pw*4,*b=s->planes+(size_t)yn*pw*4;
  for(unsigned x=0;x<pw;x++){
   unsigned xn=x+1<pw?x+1:x;
   out[2*x]=a[4*x];
   out[2*x+1]=(uint16_t)(((uint32_t)a[4*x+1]+a[4*xn+1]+1)/2);
   out[F3_WIDTH+2*x]=(uint16_t)(((uint32_t)a[4*x+3]+b[4*x+3]+1)/2);
   out[F3_WIDTH+2*x+1]=(uint16_t)(((uint32_t)a[4*x+2]+a[4*xn+2]+b[4*x+2]+b[4*xn+2]+2)/4);
  }
 }
}
static unsigned bounded_rows(unsigned requested,uint64_t budget){
 /* 输入四份完整行 + RGBG 双倍网格。含一行 halo；输出固定64行。
  * 容量随块行数线性增长；没有整幅内存分配。 */
 uint64_t per=(uint64_t)4*11904*2+(uint64_t)SIXTEEN_WIDTH*32;
 uint64_t reserve=sizeof m+per+(uint64_t)F3_WIDTH*64*2+UINT64_C(8)*1024*1024;
 if(budget<=reserve)return 0;
 uint64_t rows=(budget-reserve)/per;if(rows>requested)rows=requested;
 if(rows>SIXTEEN_HEIGHT)rows=SIXTEEN_HEIGHT;
 return (unsigned)rows;
}
int main(int argc,char **argv){
 int rc=1,owned=0,pool=0;FILE *out=NULL;unsigned requested=1024,budget_mib=512;
 if(argc!=3&&argc!=5){fprintf(stderr,"expected output, owned job, [rows budget_MiB]\n");return 2;}
 if(argc==5){char *a,*b;unsigned long r=strtoul(argv[3],&a,10),budget=strtoul(argv[4],&b,10);
  if(*a||*b||!r||r>1024||budget<32||budget>1024)return 2;requested=(unsigned)r;budget_mib=(unsigned)budget;}
 if((uint64_t)SIXTEEN_WIDTH+126>11904||(uint64_t)SIXTEEN_HEIGHT+93>8842||
    (uint64_t)F3_WIDTH*F3_HEIGHT*2>UINT32_MAX){
  fputs("SIXTEEN_OUTPUT_GEOMETRY_REJECTED\n",stderr);return 2;
 }
 uint64_t budget=(uint64_t)budget_mib*1024*1024;
#ifndef _WIN32
 FILE *mem=fopen("/proc/meminfo","r");if(mem){char line[192];
  while(fgets(line,sizeof line,mem))if(!strncmp(line,"MemAvailable:",13)){
   char *end;unsigned long long kb=strtoull(line+13,&end,10);
   if(end!=line+13&&kb<=UINT64_MAX/1024){
    uint64_t safe=kb>65536?(kb-65536)*1024:0;if(safe<budget)budget=safe;
   }
   break;}
  fclose(mem);}
#endif
 m.batch=bounded_rows(requested,budget);if(!m.batch)goto end;
 m.plane_width=2*SIXTEEN_WIDTH;m.plane_rows=2*(m.batch+1);
 for(unsigned i=0;i<16;i++){char path[512];int n=snprintf(path,sizeof path,"%s/frame-%u.3FR",argv[2],i);
  if(n<1||n>=(int)sizeof path||!validate_source(i,path))goto end;}
 uint8_t descriptor[4096];const char *reason=NULL;
 if(!capture_merged_header_checked(m.headers[0],descriptor,&reason)||
    !factory_native_3fr_header_checked(m.headers[0],descriptor,16,m.preview_bytes,m.final_header,&reason)){
  if(reason)fputs(reason,stderr);goto end;}
 for(unsigned i=0;i<4;i++)if(!(m.scratch[i]=malloc((size_t)(m.batch+1)*11904*2)))goto end;
 m.planes=malloc((size_t)m.plane_width*m.plane_rows*8);
 m.rows=malloc((size_t)F3_WIDTH*64*2);if(!m.planes||!m.rows)goto end;
 if(!merge_workers_start())goto end;pool=1;
 {int fd=open(argv[1],O_CREAT|O_EXCL|O_WRONLY|O_BINARY|O_NOFOLLOW,0600);
  if(fd<0)goto end;owned=1;out=fdopen(fd,"wb");if(!out){close(fd);goto end;}}
 if(fwrite(m.final_header,1,F3_HEADER,out)!=F3_HEADER)goto end;
 printf("SIXTEEN_DISK_ROWS requested=%u actual=%u workers=4 quality=60 capacity=536870912\n",requested,m.batch);
 for(m.top=0;m.top<SIXTEEN_HEIGHT;m.top+=m.count){
  m.count=SIXTEEN_HEIGHT-m.top;if(m.count>m.batch)m.count=m.batch;
  m.read_count=m.count+(m.top+m.count<SIXTEEN_HEIGHT);
  merge_workers_run(16,load_range,&m);
  for(unsigned i=0;i<4;i++)if(m.errors[i])goto end;
  for(m.write_y=0;m.write_y<2*m.count;m.write_y+=m.write_count){
   m.write_count=2*m.count-m.write_y;if(m.write_count>32)m.write_count=32;
   merge_workers_run(m.write_count,remosaic_range,&m);
   size_t samples=(size_t)m.write_count*2*F3_WIDTH;
   if(fwrite(m.rows,2,samples,out)!=samples)goto end;
  }
  printf("SIXTEEN_DISK_MERGED_ROWS %u/%u\n",4*(m.top+m.count),F3_HEIGHT);fflush(stdout);
 }
 if(!seek_at(m.files[0],m.preview_offset))goto end;
 {uint8_t buffer[65536];uint32_t remaining=m.preview_bytes;
  while(remaining){size_t n=remaining<sizeof buffer?remaining:sizeof buffer;
   if(fread(buffer,1,n,m.files[0])!=n||fwrite(buffer,1,n,out)!=n)goto end;remaining-=(uint32_t)n;}}
 {int64_t n=ftello(out);if(n!=(int64_t)F3_HEADER+F3_RAW_BYTES+m.preview_bytes)goto end;
  unsigned pad=(unsigned)((4096-(n&4095))&4095);uint8_t zero[4096]={0};
  if(pad&&fwrite(zero,1,pad,out)!=pad)goto end;}
 if(fflush(out)||fsync(fileno(out)))goto end;
 rc=0;
end:
 if(pool)merge_workers_stop();
 if(out&&fclose(out))rc=1;
 for(unsigned i=0;i<16;i++)if(m.files[i]&&fclose(m.files[i]))rc=1;
 for(unsigned i=0;i<4;i++)free(m.scratch[i]);free(m.planes);free(m.rows);
 if(rc&&owned)remove(argv[1]);
 if(!rc)puts("SIXTEEN_DISK_RAW_FSYNC_4K_COMPLETE_INPUTS_RETAINED");
 else fputs("SIXTEEN_DISK_MERGE_FAILED_INPUTS_RETAINED\n",stderr);
 return rc;
}
