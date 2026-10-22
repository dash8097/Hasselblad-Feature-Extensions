/* 修改本次合成成片的 TIFF Model 字段；不扫描或改动私有标定数据。 */
#ifndef FACTORY_OVERCLOCK_MODEL_H
#define FACTORY_OVERCLOCK_MODEL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static int factory_overclock_model(uint8_t *data,size_t bytes){
 if(!data||bytes<8||memcmp(data,"II*\0",4))return 0;
 uint32_t directory=(uint32_t)data[4]|(uint32_t)data[5]<<8|(uint32_t)data[6]<<16|(uint32_t)data[7]<<24;
 if(directory<8||directory+2>bytes)return 0;
 unsigned count=data[directory]|(unsigned)data[directory+1]<<8;
 if(count>128||(uint64_t)directory+2+12*count+4>bytes)return 0;
 uint8_t *model=NULL;uint32_t length=0;
 for(unsigned i=0;i<count;i++){
  uint8_t *p=data+directory+2+12*i;
  if(p[0]!=16||p[1]!=1)continue;
  if(model||p[2]!=2||p[3])return 0;
  length=(uint32_t)p[4]|(uint32_t)p[5]<<8|(uint32_t)p[6]<<16|(uint32_t)p[7]<<24;
  uint32_t offset=length<=4?(uint32_t)(p-data)+8:
   (uint32_t)p[8]|(uint32_t)p[9]<<8|(uint32_t)p[10]<<16|(uint32_t)p[11]<<24;
  if(length<9||length>64||(uint64_t)offset+length>bytes)return 0;model=data+offset;
 }
 if(!model||!memchr(model,0,length))return 0;
 for(uint32_t i=0;i+9<=length;i++){
  if((i==0||model[i-1]==' ')&&!memcmp(model+i,"X2D ",4)&&
     (model[i+4]=='1'||model[i+4]=='4')&&!memcmp(model+i+5,"00C\0",4)){
   model[i+4]='4';return 1;
  }
 }
 return 0;
}
#endif
