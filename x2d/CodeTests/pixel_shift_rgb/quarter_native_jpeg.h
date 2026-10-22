/* 原厂 JPEG 的 APP 元数据保留；只改成片尺寸，不读取或记录拍摄信息。
 * 接收原厂压缩流，元数据整理后仍交给原厂 FileIO 写入。 */
#ifndef QUARTER_NATIVE_JPEG_H
#define QUARTER_NATIVE_JPEG_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static unsigned qj_u16(const uint8_t *p,int be){return be?(unsigned)p[0]*256+p[1]:p[0]+(unsigned)p[1]*256;}
static uint32_t qj_u32(const uint8_t *p,int be){return be?((uint32_t)qj_u16(p,1)<<16)|qj_u16(p+2,1):qj_u16(p,0)|((uint32_t)qj_u16(p+2,0)<<16);}
static void qj_put(uint8_t *p,uint32_t v,int be,unsigned size){for(unsigned i=0;i<size;i++)p[be?size-i-1:i]=(uint8_t)(v>>(8*i));}
static int qj_ifd(uint8_t *t,size_t n,uint32_t offset,int be,unsigned w,unsigned h,unsigned depth){
 if(!offset)return 1;
 if(depth>2||offset<8||offset>n||n-offset<6)return 0;
 unsigned count=qj_u16(t+offset,be);if(count>256||6u+12u*count>n-offset)return 0;
 for(unsigned i=0;i<count;i++){
  uint8_t *p=t+offset+2+12*i;unsigned tag=qj_u16(p,be),type=qj_u16(p+2,be);
  if(tag==34665){if(type!=4||qj_u32(p+4,be)!=1||!qj_ifd(t,n,qj_u32(p+8,be),be,w,h,depth+1))return 0;}
  if(tag==256||tag==257||tag==40962||tag==40963){
   if(qj_u32(p+4,be)!=1||(type!=3&&type!=4))return 0;
   qj_put(p+8,(tag==256||tag==40962)?w:h,be,type==3?2:4);
  }
 }
 /* 不遍历 IFD1：嵌入缩略图保持原厂尺寸。 */
 return 1;
}
static int qj_prefix(uint8_t *p,size_t n,unsigned w,unsigned h){
 if(!p||n<2||p[0]!=255||p[1]!=216||!w||!h)return 0;
 size_t at=2;
 while(at<n){
  if(n-at<4||p[at]!=255||p[at+1]<0xe0||p[at+1]>0xef)return 0;
  size_t length=qj_u16(p+at+2,1);if(length<2||length+2>n-at)return 0;
  if(p[at+1]==0xe1&&length>=16&&!memcmp(p+at+4,"Exif\0\0",6)){
   uint8_t *t=p+at+10;size_t bytes=length-8;
   int be=!memcmp(t,"MM",2);
   if((!be&&memcmp(t,"II",2))||qj_u16(t+2,be)!=42||!qj_ifd(t,bytes,qj_u32(t+4,be),be,w,h,0))return 0;
  }
  at+=length+2;
 }
 return at==n;
}
/* 4.2.0 createAlignmentBlock(0x158c28)使用COM(0xfffe)，可能延伸到原JPEG负载；不能
 * 原样接到缩小后的码流。保留完整Exif和ICC及其他完整APP标记。 */
static size_t qj_copy_prefix(const uint8_t *source,size_t n,uint8_t *out,size_t capacity,unsigned w,unsigned h){
 if(!source||!out||n<2||capacity<2||source[0]!=255||source[1]!=216)return 0;
 out[0]=255;out[1]=216;size_t at=2,written=2;
 while(at<n){
  if(n-at<4||source[at]!=255||((source[at+1]<0xe0||source[at+1]>0xef)&&source[at+1]!=0xfe))return 0;
  size_t length=qj_u16(source+at+2,1);if(length<2)return 0;
  if(length+2>n-at){if(source[at+1]==0xe2||source[at+1]==0xfe)break;return 0;}
  int keep=source[at+1]!=0xfe&&(source[at+1]!=0xe2||(length>=16&&!memcmp(source+at+4,"ICC_PROFILE\0",12)));
  if(keep){if(length+2>capacity-written)return 0;memcpy(out+written,source+at,length+2);written+=length+2;}
  at+=length+2;
 }
 return qj_prefix(out,written,w,h)?written:0;
}
/* 只走 JPEG 标记，定位有效 EOI 和 SOF；熵编码区不逐像素解码。 */
static size_t qj_stream(const uint8_t *p,size_t n,unsigned *width,unsigned *height){
 if(!p||n<4||p[0]!=255||p[1]!=216)return 0;
 size_t at=2;int scan=0;unsigned w=0,h=0;
 while(at<n){
  if(scan){
   while(at<n&&p[at]!=255)at++;
   if(at==n)return 0;
  }else if(p[at]!=255)return 0;
  while(at<n&&p[at]==255)at++;
  if(at==n)return 0;
  unsigned marker=p[at++];
  if(marker==0&&scan)continue;
  if(marker>=0xd0&&marker<=0xd7&&scan)continue;
  if(marker==0xd9){if(!w||!h)return 0;if(width)*width=w;if(height)*height=h;return at;}
  scan=0;
  if(n-at<2)return 0;size_t length=qj_u16(p+at,1);
  if(length<2||length>n-at)return 0;
  if((marker>=0xc0&&marker<=0xc3)||(marker>=0xc5&&marker<=0xc7)||(marker>=0xc9&&marker<=0xcb)||(marker>=0xcd&&marker<=0xcf)){
   if(length<8||w||h)return 0;h=qj_u16(p+at+3,1);w=qj_u16(p+at+5,1);
  }
  at+=length;if(marker==0xda)scan=1;
 }
 return 0;
}
/* NV16编码器要求偶数行。只允许25MP末尾复制行4372->4371，且MCU行数
 * 不变；不裁像素、不解码、不重编码。只修改调用者拥有的私有副本。 */
static int qj_quarter_visible(uint8_t *p,size_t n){
 unsigned w=0,h=0;
 if(!qj_stream(p,n,&w,&h)||w!=5828||(h!=4371&&h!=4372))return 0;
 if(h==4371)return 1;
 size_t at=2;
 while(at+4<=n){
  if(p[at++]!=255)return 0;
  while(at<n&&p[at]==255)at++;
  if(at+3>n)return 0;
  unsigned marker=p[at++],len=qj_u16(p+at,1);
  if(len<2||len>n-at||marker==0xda)return 0;
  if(marker==0xc0){
   if(len!=17||p[at+2]!=8||p[at+7]!=3)return 0;
   unsigned maxv=0;
   for(unsigned i=0;i<3;i++){
    unsigned sampling=p[at+9+3*i],v=sampling&15;
    if(!v||v>2||!(sampling>>4)||(sampling>>4)>2)return 0;
    if(v>maxv)maxv=v;
   }
   unsigned mcu=8*maxv;
   if((4371+mcu-1)/mcu!=(4372+mcu-1)/mcu)return 0;
   qj_put(p+at+3,4371,1,2);return 1;
  }
  at+=len;
 }
 return 0;
}
#endif
