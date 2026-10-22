/* 基线 JPEG 行带合并：只解析小头部，原封保留压缩扫描数据。
 * 每个行带独立编码，插入 DRI/RST 使 DC 状态在行带边界归零；不解码或重编码 JPEG。
 */
#ifndef FACTORY_JPEG_BAND_HEADER_H
#define FACTORY_JPEG_BAND_HEADER_H
#include <stdint.h>
#include <stddef.h>
#define FJB_HEADER_MAX 4096u
typedef struct {uint32_t scan,sof_height,sos,mcu_width,mcu_height,width,height;} FjbHeader;
static unsigned fjb16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static int fjb_parse(const uint8_t *p,uint32_t n,FjbHeader *out){
 if(!p||!out||n<4||p[0]!=255||p[1]!=216||p[n-2]!=255||p[n-1]!=217)return 0;
 FjbHeader h={0};unsigned pos=2;
 while(pos+4<=n&&pos<FJB_HEADER_MAX){
  if(p[pos]!=255)return 0;unsigned marker=p[pos+1],len=fjb16(p+pos+2);
  if(len<2||len>n-pos-2||pos+2+len>FJB_HEADER_MAX)return 0;
  if(marker==0xc0){
   if(h.sof_height||len!=17||p[pos+4]!=8||p[pos+9]!=3)return 0;
   h.height=fjb16(p+pos+5);h.width=fjb16(p+pos+7);h.sof_height=pos+5;
   unsigned horizontal=p[pos+11]>>4,vertical=p[pos+11]&15;
   if(horizontal!=2||(vertical!=1&&vertical!=2)||p[pos+14]!=0x11||p[pos+17]!=0x11)return 0;
   h.mcu_width=horizontal*8;h.mcu_height=vertical*8;
  }else if(marker==0xdd){if(len!=4||fjb16(p+pos+4))return 0;}
  else if(marker==0xda){
   if(!h.sof_height||len!=12||p[pos+4]!=3||p[pos+11]!=0||p[pos+12]!=63||p[pos+13]!=0)return 0;
   h.sos=pos;h.scan=pos+len+2;*out=h;return h.scan<n-2;
  }else if(marker==0xc4||marker==0xdb||marker==0xfe||(marker>=0xe0&&marker<=0xef)){}
  else return 0;
  pos+=len+2;
 }
 return 0;
}
static int fjb_same_header(const uint8_t *first,const FjbHeader *a,const uint8_t *next,const FjbHeader *b){
 if(a->scan!=b->scan||a->sof_height!=b->sof_height||a->sos!=b->sos||a->width!=b->width||
    a->mcu_width!=b->mcu_width||a->mcu_height!=b->mcu_height)return 0;
 for(unsigned i=0;i<a->scan;i++)if(i!=a->sof_height&&i!=a->sof_height+1&&first[i]!=next[i])return 0;
 return 1;
}
static int fjb_first_header(uint8_t *dest,uint32_t capacity,const uint8_t *source,const FjbHeader *h,unsigned total_height,unsigned band_rows){
 if(!dest||!source||!h||!h->mcu_height||!h->mcu_width||!total_height||total_height>65535||
    band_rows%h->mcu_height||capacity<h->scan+6)return 0;
 unsigned interval=((h->width+h->mcu_width-1)/h->mcu_width)*(band_rows/h->mcu_height);
 if(!interval||interval>65535)return 0;
 for(unsigned i=0;i<h->sos;i++)dest[i]=source[i];
 dest[h->sof_height]=(uint8_t)(total_height>>8);dest[h->sof_height+1]=(uint8_t)total_height;
 const uint8_t dri[6]={255,221,0,4,(uint8_t)(interval>>8),(uint8_t)interval};
 for(unsigned i=0;i<6;i++)dest[h->sos+i]=dri[i];
 for(unsigned i=h->sos;i<h->scan;i++)dest[i+6]=source[i];
 return (int)(h->scan+6);
}
#endif
