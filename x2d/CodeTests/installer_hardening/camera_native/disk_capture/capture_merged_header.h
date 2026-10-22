/* 内部合成描述头；最终 3FR 的拍摄 metadata 仍来自真实首帧头部。
 * 不创建 DNG 文件，不用合成常量替代颜色矩阵、白平衡或黑电平。 */
#ifndef CAPTURE_MERGED_HEADER_H
#define CAPTURE_MERGED_HEADER_H
#include "factory_native_3fr.h"
static inline int capture_merged_header_checked(const uint8_t source[16384],uint8_t out[4096],const char **reason){
 if(reason)*reason="MEMORY_OUTPUT_MERGED_HEADER_REJECTED\n";
 if(!source||!out||memcmp(source,"II*\0",4))return 0;
 uint8_t original[16384],result[4096]={0};memcpy(original,source,sizeof original);
 uint32_t main=f3_u32(original+4),raw;
 if(!f3_scalar(original,sizeof original,main,330,&raw))return 0;
 uint8_t *black=f3_data(original,sizeof original,f3_tag(original,sizeof original,raw,50714),5,1,8);
 uint8_t *matrix=f3_data(original,sizeof original,f3_tag(original,sizeof original,main,50721),10,9,8);
 uint8_t *balance=f3_data(original,sizeof original,f3_tag(original,sizeof original,main,50728),5,3,8);
 if(!black||f3_u32(black+4)!=1||f3_u32(black)>8192||!matrix||!balance)return 0;
 uint32_t white;
 if(reason)*reason="MEMORY_RAW_SOURCE_WHITE_LEVEL_REJECTED\n";
 if(!f3_scalar(original,sizeof original,raw,50717,&white)||white<=f3_u32(black)||white>65535)return 0;
 memcpy(result,"II*\0",4);f3_p32(result+4,8);f3_p16(result+8,13);
 const unsigned tags[]={256,257,258,259,262,273,277,279,50714,50717};
 const uint32_t values[]={F3_WIDTH,F3_HEIGHT,16,1,32803,4096,1,F3_RAW_BYTES,f3_u32(black),white};
 for(unsigned i=0;i<10;i++){
  uint8_t *entry=result+10+12*i;f3_p16(entry,(uint16_t)tags[i]);f3_p16(entry+2,4);
  f3_p32(entry+4,1);f3_p32(entry+8,values[i]);
 }
 uint8_t *entry=result+130;f3_p16(entry,33422);f3_p16(entry+2,1);f3_p32(entry+4,4);
#ifdef CAPTURE_MERGED_RGGB
 const uint8_t phase[4]={0,1,1,2};
#else
 const uint8_t phase[4]={1,0,2,1};
#endif
 memcpy(entry+8,phase,4);
 entry+=12;f3_p16(entry,50721);f3_p16(entry+2,10);f3_p32(entry+4,9);f3_p32(entry+8,512);memcpy(result+512,matrix,72);
 entry+=12;f3_p16(entry,50728);f3_p16(entry+2,5);f3_p32(entry+4,3);f3_p32(entry+8,584);memcpy(result+584,balance,24);
 for(unsigned i=0;i<13;i++)for(unsigned j=i+1;j<13;j++){
  uint8_t *a=result+10+12*i,*b=result+10+12*j;
  if(f3_u16(a)>f3_u16(b)){uint8_t copy[12];memcpy(copy,a,12);memcpy(a,b,12);memcpy(b,copy,12);}
 }
 memcpy(out,result,sizeof result);if(reason)*reason=NULL;return 1;
}
static inline int capture_merged_header(const uint8_t source[16384],uint8_t out[4096]){
 return capture_merged_header_checked(source,out,NULL);
}
#endif
