/* 原厂数值域四亿合成 RAW 的容器头。仅处理文件，不登记相册或控制设备。
 * 保留原厂私有标定；成片裁切改为四亿有效区域。空间映射仍待验证。
 */
#ifndef FACTORY_NATIVE_3FR_H
#define FACTORY_NATIVE_3FR_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define F3_HEADER 16384u
#ifndef F3_WIDTH
#define F3_WIDTH 23326u
#endif
#ifndef F3_HEIGHT
#define F3_HEIGHT 17498u
#endif
#define F3_RAW_BYTES (F3_WIDTH*F3_HEIGHT*2u)
static uint16_t f3_u16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t f3_u32(const uint8_t *p){return f3_u16(p)|(uint32_t)f3_u16(p+2)<<16;}
static void f3_p16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void f3_p32(uint8_t *p,uint32_t v){f3_p16(p,(uint16_t)v);f3_p16(p+2,(uint16_t)(v>>16));}
static uint8_t *f3_tag(uint8_t *b,size_t bytes,uint32_t ifd,unsigned wanted){
 if(ifd<8||ifd>bytes||bytes-ifd<6)return NULL;
 unsigned count=f3_u16(b+ifd);
 if(count>128||(uint64_t)ifd+6+12*count>bytes)return NULL;
 uint8_t *found=NULL;
 for(unsigned i=0;i<count;i++){
  uint8_t *p=b+ifd+2+12*i;
  if(f3_u16(p)==wanted){if(found)return NULL;found=p;}
 }
 return found;
}
static uint8_t *f3_data(uint8_t *b,size_t bytes,uint8_t *t,unsigned type,unsigned count,unsigned unit){
 if(!t||f3_u16(t+2)!=type||f3_u32(t+4)!=count)return NULL;
 uint64_t n=(uint64_t)count*unit;
 uint32_t offset=n<=4?(uint32_t)(t-b)+8:f3_u32(t+8);
 if((uint64_t)offset+n>bytes)return NULL;
 return b+offset;
}
static int f3_scalar(uint8_t *b,size_t bytes,uint32_t ifd,unsigned tag,uint32_t *out){
 uint8_t *t=f3_tag(b,bytes,ifd,tag);if(!t||f3_u32(t+4)!=1)return 0;
 if(f3_u16(t+2)==3)*out=f3_u16(t+8);
 else if(f3_u16(t+2)==4)*out=f3_u32(t+8);
 else return 0;
 return 1;
}
static int f3_is(uint8_t *b,size_t bytes,uint32_t ifd,unsigned tag,uint32_t expected){
 uint32_t v;return f3_scalar(b,bytes,ifd,tag,&v)&&v==expected;
}
static int f3_put(uint8_t *b,uint32_t ifd,unsigned tag,unsigned type,unsigned count,
                  const uint8_t *value,unsigned unit){
 uint8_t *p=f3_data(b,F3_HEADER,f3_tag(b,F3_HEADER,ifd,tag),type,count,unit);
 if(!p)return 0;memcpy(p,value,(size_t)count*unit);return 1;
}
static int f3_put32(uint8_t *b,uint32_t ifd,unsigned tag,uint32_t value){
 uint8_t *t=f3_tag(b,F3_HEADER,ifd,tag),encoded[4];
 if(!t||f3_u32(t+4)!=1)return 0;
 unsigned type=f3_u16(t+2),unit=type==3?2:4;
 if((type!=3&&type!=4)||(type==3&&value>65535))return 0;
 f3_p32(encoded,value);return f3_put(b,ifd,tag,type,1,encoded,unit);
}
static int f3_put_pair(uint8_t *b,uint32_t ifd,unsigned tag,uint32_t x,uint32_t y){
 uint8_t *t=f3_tag(b,F3_HEADER,ifd,tag),encoded[8];
 if(!t||f3_u32(t+4)!=2)return 0;
 unsigned type=f3_u16(t+2),unit=type==3?2:4;
 if((type!=3&&type!=4)||(type==3&&(x>65535||y>65535)))return 0;
 f3_p32(encoded,x);f3_p32(encoded+unit,y);
 return f3_put(b,ifd,tag,type,2,encoded,unit);
}
static int f3_omit(uint8_t *b,uint32_t ifd,unsigned wanted){
 if(!f3_tag(b,F3_HEADER,ifd,wanted))return 1;
 unsigned n=f3_u16(b+ifd),kept=0;
 for(unsigned i=0;i<n;i++)if(f3_u16(b+ifd+2+12*i)!=wanted){
  memmove(b+ifd+2+12*kept,b+ifd+2+12*i,12);kept++;
 }
 memmove(b+ifd+2+12*kept,b+ifd+2+12*n,4);f3_p16(b+ifd,(uint16_t)kept);return 1;
}
/* 空白字节也可能是有效元数据。先标记目录和引用区，再选完整空白段。 */
typedef struct {uint8_t used[F3_HEADER];uint32_t seen[16];unsigned count;} F3Occupied;
static int f3_mark(F3Occupied *m,uint32_t offset,uint64_t bytes){
 if(offset>=F3_HEADER)return 1;
 if(bytes>F3_HEADER-offset)return 0;
 memset(m->used+offset,1,(size_t)bytes);return 1;
}
static int f3_mark_ifd(const uint8_t *b,uint32_t ifd,F3Occupied *m){
 if(!ifd)return 1;
 if(ifd<8||ifd>F3_HEADER-6||m->count==16)return 0;
 for(unsigned i=0;i<m->count;i++)if(m->seen[i]==ifd)return 0;
 m->seen[m->count++]=ifd;
 unsigned n=f3_u16(b+ifd);
 if(n>128||!f3_mark(m,ifd,6u+12u*n))return 0;
 for(unsigned i=0;i<n;i++){
  const uint8_t *t=b+ifd+2+12*i;
  unsigned tag=f3_u16(t),type=f3_u16(t+2),unit;
  switch(type){
   case 1:case 2:case 6:case 7:unit=1;break;
   case 3:case 8:unit=2;break;
   case 4:case 9:case 11:case 13:unit=4;break;
   case 5:case 10:case 12:unit=8;break;
   default:return 0;
  }
  uint64_t bytes=(uint64_t)f3_u32(t+4)*unit;
  if(bytes>4&&!f3_mark(m,f3_u32(t+8),bytes))return 0;
  if(tag==330||tag==34665||tag==34853||tag==40965){
   if((type!=4&&type!=13)||f3_u32(t+4)!=1||!f3_mark_ifd(b,f3_u32(t+8),m))return 0;
  }else if(tag==37500){
   // 已核对的 4.2.0 MakerNote 从值偏移处开始原厂私有 IFD。
   if(!f3_mark_ifd(b,f3_u32(t+8),m))return 0;
  }
 }
 return f3_mark_ifd(b,f3_u32(b+ifd+2+12*n),m);
}
static int f3_free_span(const uint8_t *b,const F3Occupied *m,uint32_t offset,unsigned bytes){
 if(offset<8||offset>F3_HEADER||bytes>F3_HEADER-offset)return 0;
 for(unsigned i=offset;i<offset+bytes;i++)if(b[i]||m->used[i])return 0;
 return 1;
}
static uint32_t f3_relocation(const uint8_t *b,uint32_t first,unsigned bytes){
 F3Occupied m={0};memset(m.used,1,8);
 if(bytes>F3_HEADER-8||!f3_mark_ifd(b,first,&m))return 0;
 if(f3_free_span(b,&m,15872,bytes))return 15872;
 for(uint32_t offset=(F3_HEADER-bytes)&~1u;offset>=8;offset-=2)
  if(f3_free_span(b,&m,offset,bytes))return offset;
 return 0;
}
/* 成功才写入 out。拒绝不同数值域、非首帧、未知布局及没有安全头部空间。 */
static inline int factory_native_3fr_header_checked(const uint8_t *source,const uint8_t *merged,
 unsigned frames,uint32_t preview_bytes,uint8_t *out,const char **reason){
 if(reason)*reason="MEMORY_RAW_HEADER_ARGUMENT_REJECTED\n";
 if(!source||!merged||!out||(frames!=4&&frames!=6&&frames!=16)||!preview_bytes||
    preview_bytes>16u*1024*1024||memcmp(source,"II\052\000",4)||memcmp(merged,"II\052\000",4))return 0;
 uint8_t original[F3_HEADER],input[4096],candidate[F3_HEADER];
 memcpy(original,source,sizeof original);memcpy(input,merged,sizeof input);
 uint32_t first=f3_u32(original+4),raw,exif=0,mi=f3_u32(input+4);
 if(reason)*reason="MEMORY_RAW_HEADER_DIRECTORY_REJECTED\n";
 if(!f3_scalar(original,sizeof original,first,330,&raw))return 0;
 if(f3_tag(original,sizeof original,first,34665)&&!f3_scalar(original,sizeof original,first,34665,&exif))return 0;
 const unsigned tags[]={256,257,258,259,262,277,273,279};
 const uint32_t old[]={11904,8842,16,1,32803,1,F3_HEADER,210510336};
 const uint32_t current[]={F3_WIDTH,F3_HEIGHT,16,1,32803,1,4096,F3_RAW_BYTES};
 const char *source_errors[]={
  "MEMORY_RAW_SOURCE_WIDTH_REJECTED\n","MEMORY_RAW_SOURCE_HEIGHT_REJECTED\n",
  "MEMORY_RAW_SOURCE_BIT_DEPTH_REJECTED\n","MEMORY_RAW_SOURCE_COMPRESSION_REJECTED\n",
  "MEMORY_RAW_SOURCE_PHOTOMETRIC_REJECTED\n","MEMORY_RAW_SOURCE_SAMPLES_REJECTED\n",
  "MEMORY_RAW_SOURCE_STRIP_OFFSET_REJECTED\n","MEMORY_RAW_SOURCE_STRIP_BYTES_REJECTED\n"};
 const char *merged_errors[]={
  "MEMORY_RAW_MERGED_WIDTH_REJECTED\n","MEMORY_RAW_MERGED_HEIGHT_REJECTED\n",
  "MEMORY_RAW_MERGED_BIT_DEPTH_REJECTED\n","MEMORY_RAW_MERGED_COMPRESSION_REJECTED\n",
  "MEMORY_RAW_MERGED_PHOTOMETRIC_REJECTED\n","MEMORY_RAW_MERGED_SAMPLES_REJECTED\n",
  "MEMORY_RAW_MERGED_STRIP_OFFSET_REJECTED\n","MEMORY_RAW_MERGED_STRIP_BYTES_REJECTED\n"};
 for(unsigned i=0;i<sizeof tags/sizeof tags[0];i++){
  if(reason)*reason=source_errors[i];
  if(!f3_is(original,sizeof original,raw,tags[i],old[i]))return 0;
  if(reason)*reason=merged_errors[i];
  if(!f3_is(input,sizeof input,mi,tags[i],current[i]))return 0;
 }
 if(reason)*reason="MEMORY_RAW_SOURCE_PREVIEW_COMPRESSION_REJECTED\n";
 if(!f3_is(original,sizeof original,first,259,7))return 0;
 if(reason)*reason="MEMORY_RAW_HEADER_GROUP_IDENTITY_REJECTED\n";
 /* 与实时头部导出器接受的三个目录一致；多个 MakerNote 拒绝歧义。
  * 保留原始目录位置和内容，不将元数据搬到另一个目录。 */
 uint8_t *maker=NULL;uint32_t dirs[3]={raw,first,exif};
 for(unsigned i=0;i<3;i++){
  if(!dirs[i])continue;
  for(unsigned j=0;j<i;j++)if(dirs[i]==dirs[j])return 0;
  uint8_t *entry=f3_tag(original,sizeof original,dirs[i],37500);
  if(entry){if(maker)return 0;maker=entry;}
 }
 uint8_t *group=maker?f3_data(original,sizeof original,
  f3_tag(original,sizeof original,f3_u32(maker+8),33),9,4,4):NULL;
 if(!group||f3_u32(group)!=frames||f3_u32(group+4)!=0)return 0;
 if(reason)*reason="MEMORY_RAW_HEADER_BLACK_LEVEL_REJECTED\n";
 uint8_t *black=f3_data(original,sizeof original,f3_tag(original,sizeof original,raw,50714),5,1,8);
 uint32_t merged_black;
 if(!black||f3_u32(black+4)!=1||f3_u32(black)>8192||
    !f3_scalar(input,sizeof input,mi,50714,&merged_black)||merged_black!=f3_u32(black))return 0;
 /* 官方 4.2.0 setSensorDigitalGain 会动态写 WhiteLevel（tag 50717）。
  * 合成没有重标定像素，描述头与最终 RAW 必须沿用首帧数值域。
  * 只检查头部的一个标量，不扫描或校验任何像素。 */
 uint32_t source_white,merged_white;
 if(reason)*reason="MEMORY_RAW_SOURCE_WHITE_LEVEL_REJECTED\n";
 if(!f3_scalar(original,sizeof original,raw,50717,&source_white)||
    source_white<=f3_u32(black)||source_white>65535)return 0;
 if(reason)*reason="MEMORY_RAW_MERGED_WHITE_LEVEL_REJECTED\n";
 if(!f3_scalar(input,sizeof input,mi,50717,&merged_white)||merged_white!=source_white)return 0;
 if(reason)*reason="MEMORY_RAW_HEADER_COLOR_FIELDS_REJECTED\n";
 for(unsigned i=0;i<2;i++){
  unsigned tag=i?50728:50721,count=i?3:9,type=i?5:10;
  uint8_t *a=f3_data(original,sizeof original,f3_tag(original,sizeof original,first,tag),type,count,8);
  uint8_t *b=f3_data(input,sizeof input,f3_tag(input,sizeof input,mi,tag),type,count,8);
  if(!a||!b||memcmp(a,b,8*count))return 0;
 }
 if(reason)*reason="MEMORY_RAW_HEADER_CFA_REJECTED\n";
 uint8_t *cfa=f3_data(input,sizeof input,f3_tag(input,sizeof input,mi,33422),1,4,1);
 static const uint8_t phases[2][4]={{0,1,1,2},{1,0,2,1}};
 if(!cfa||memcmp(cfa,phases[frames==6],4)||
    f3_tag(original,sizeof original,raw,33421)||f3_tag(original,sizeof original,raw,33422))return 0;
 if(reason)*reason="MEMORY_RAW_HEADER_RELOCATION_REJECTED\n";
 unsigned n=f3_u16(original+raw);
 if(n>38)return 0;
 const unsigned relocated=f3_relocation(original,first,6+12*(n+2));
 if(!relocated)return 0;
 memcpy(candidate,original,sizeof candidate);
 f3_p16(candidate+relocated,(uint16_t)(n+2));
 memcpy(candidate+relocated+2,original+raw+2,12*n);
 memcpy(candidate+relocated+2+12*(n+2),original+raw+2+12*n,4);
 uint8_t *a=candidate+relocated+2+12*n,*b=a+12;
 f3_p16(a,33421);f3_p16(a+2,3);f3_p32(a+4,2);f3_p16(a+8,2);f3_p16(a+10,2);
 f3_p16(b,33422);f3_p16(b+2,1);f3_p32(b+4,4);memcpy(b+8,cfa,4);
 for(unsigned i=0;i<n+2;i++)for(unsigned j=i+1;j<n+2;j++){
  a=candidate+relocated+2+12*i;b=candidate+relocated+2+12*j;
  if(f3_u16(a)>f3_u16(b)){uint8_t t[12];memcpy(t,a,12);memcpy(a,b,12);memcpy(b,t,12);}
 }
 if(reason)*reason="MEMORY_RAW_HEADER_OUTPUT_FIELDS_REJECTED\n";
 if(!f3_put32(candidate,first,330,relocated)||
    !f3_put32(candidate,relocated,256,F3_WIDTH)||!f3_put32(candidate,relocated,257,F3_HEIGHT)||
    !f3_put32(candidate,relocated,278,F3_HEIGHT)||!f3_put32(candidate,relocated,279,F3_RAW_BYTES)||
    !f3_put_pair(candidate,relocated,50719,0,0)||!f3_put_pair(candidate,relocated,50720,F3_WIDTH,F3_HEIGHT)||
    !f3_put32(candidate,first,273,F3_HEADER+F3_RAW_BYTES)||
    !f3_put32(candidate,first,279,preview_bytes)||!f3_put32(candidate,first,256,3888)||
    !f3_put32(candidate,first,257,2918))return 0;
 f3_omit(candidate,relocated,50830);f3_omit(candidate,first,305);
 /* 4.2.0 Metadata::cropRectangle 优先 MakerNote 0x59，之后才看
  * DefaultCropSize。只改后者会让同名 RAW/JPEG 仍被相册报成一亿。
  * 格式为 SHORT 数组：[启用,x,y,width,height,...]；保留附加项。 */
 if(reason)*reason="MEMORY_RAW_HEADER_CROP_REJECTED\n";
 uint32_t maker_ifd=f3_u32(maker+8);
 uint8_t *crop_tag=f3_tag(candidate,sizeof candidate,maker_ifd,0x59);
 if(crop_tag){
  unsigned count=f3_u32(crop_tag+4);
  uint8_t *crop=count>=5&&count<=32?f3_data(candidate,sizeof candidate,crop_tag,3,count,2):NULL;
  if(!crop)return 0;
  f3_p16(crop,1);f3_p16(crop+2,0);f3_p16(crop+4,0);
  f3_p16(crop+6,F3_WIDTH);f3_p16(crop+8,F3_HEIGHT);
 }
 memcpy(out,candidate,sizeof candidate);if(reason)*reason=NULL;return 1;
}
static inline int factory_native_3fr_header(const uint8_t *source,const uint8_t *merged,
 unsigned frames,uint32_t preview_bytes,uint8_t *out){
 return factory_native_3fr_header_checked(source,merged,frames,preview_bytes,out,NULL);
}
#endif
