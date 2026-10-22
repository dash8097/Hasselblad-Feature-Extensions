/* 从本次第一帧的 TIFF 头复制拍摄参数，生成四亿 JPEG 的白名单 EXIF。
 * 不复制私有标定、设备序列号、GPS、旧预览或 Software；不推算曝光时间。
 * 仅接受已验证的原厂小端 TIFF 头布局。 */
#ifndef FACTORY_NATIVE_EXIF_H
#define FACTORY_NATIVE_EXIF_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {uint16_t id,type;uint32_t count;const uint8_t *value;uint32_t bytes;} FactoryExifTag;
static uint16_t fe16(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t fe32(const uint8_t *p){return fe16(p)|(uint32_t)fe16(p+2)<<16;}
static void fep16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void fep32(uint8_t *p,uint32_t v){fep16(p,(uint16_t)v);fep16(p+2,(uint16_t)(v>>16));}
static uint32_t fe_size(uint16_t type){
 switch(type){case 1:case 2:case 7:return 1;case 3:return 2;case 4:case 9:return 4;case 5:case 10:return 8;default:return 0;}
}
static int fe_ifd(const uint8_t *data,size_t bytes,uint32_t offset,FactoryExifTag *tags,unsigned *count){
 if(offset<8||offset>bytes||bytes-offset<2)return 0;
 unsigned n=fe16(data+offset);
 if(n>128||(uint64_t)offset+2+12*n+4>bytes)return 0;
 for(unsigned i=0;i<n;i++){
  const uint8_t *p=data+offset+2+12*i;FactoryExifTag *t=tags+i;
  t->id=fe16(p);t->type=fe16(p+2);t->count=fe32(p+4);t->bytes=0;t->value=NULL;
  for(unsigned j=0;j<i;j++)if(tags[j].id==t->id)return 0;
  uint64_t length=(uint64_t)fe_size(t->type)*t->count;
  if(!length||length>4096)continue;
  uint32_t start=length<=4?offset+2+12*i+8:fe32(p+8);
  if((uint64_t)start+length>bytes)continue;
  t->bytes=(uint32_t)length;t->value=data+start;
 }
 *count=n;return 1;
}
static const FactoryExifTag *fe_find(const FactoryExifTag *tags,unsigned count,uint16_t id){
 for(unsigned i=0;i<count;i++)if(tags[i].id==id&&tags[i].value)return tags+i;
 return NULL;
}
static int fe_emit(uint8_t *out,size_t capacity,uint32_t offset,
 const FactoryExifTag *tags,unsigned count,uint32_t *end){
 if((uint64_t)offset+2+12*count+4>capacity)return 0;
 fep16(out+offset,(uint16_t)count);
 for(unsigned i=0;i<count;i++){
  const FactoryExifTag *t=tags+i;uint8_t *entry=out+offset+2+12*i;
  fep16(entry,t->id);fep16(entry+2,t->type);fep32(entry+4,t->count);
  if(t->bytes<=4)memcpy(entry+8,t->value,t->bytes);
  else{
   *end=(*end+1)&~1u;
   if((uint64_t)*end+t->bytes>capacity)return 0;
   fep32(entry+8,*end);memcpy(out+*end,t->value,t->bytes);*end+=t->bytes;
  }
 }
 return 1;
}
/* out 包含完整 APP1 标记；失败时 length 保持不变。 */
static int factory_native_exif(const uint8_t *source,size_t bytes,
 uint32_t width,uint32_t height,uint8_t *out,size_t capacity,uint32_t *length){
 FactoryExifTag first[128],source_exif[128],kept[24],root[4];unsigned first_n=0,source_n=0,n=0;
 if(!source||!out||!length||bytes<8||capacity<512||!width||!height||width>65535||height>65535||
    memcmp(source,"II\052\000",4)||!fe_ifd(source,bytes,fe32(source+4),first,&first_n))return 0;
 const FactoryExifTag *ptr=fe_find(first,first_n,34665);
 if(!ptr||ptr->type!=4||ptr->count!=1||!fe_ifd(source,bytes,fe32(ptr->value),source_exif,&source_n))return 0;
 /* 手动镜头可能不报告光圈或焦距；未知镜头值不应阻止成片。 */
 const uint16_t required[]={33434,34855},types[]={5,3};
 for(unsigned i=0;i<2;i++){
  const FactoryExifTag *t=fe_find(source_exif,source_n,required[i]);
  if(!t||t->type!=types[i]||t->count!=1)return 0;
  if(t->type==5){if(!fe32(t->value)||!fe32(t->value+4))return 0;}
  else if(!fe16(t->value))return 0;
 }
 uint8_t orient[2]={1,0},pointer[4]={62,0,0,0},color[2]={1,0},w[4],h[4];
 const FactoryExifTag *orientation=fe_find(first,first_n,274);
 if(orientation){
  if(orientation->type!=3||orientation->count!=1||fe16(orientation->value)<1||fe16(orientation->value)>8)return 0;
  memcpy(orient,orientation->value,2);
 }
#ifndef FACTORY_EXIF_MODEL
#define FACTORY_EXIF_MODEL "X2D 400C"
#endif
 static const uint8_t make[]="Hasselblad",model[]=FACTORY_EXIF_MODEL;
 root[0]=(FactoryExifTag){271,2,sizeof make,make,sizeof make};
 root[1]=(FactoryExifTag){272,2,sizeof model,model,sizeof model};
 root[2]=(FactoryExifTag){274,3,1,orient,2};root[3]=(FactoryExifTag){34665,4,1,pointer,4};
 const uint16_t allow[]={33434,33437,34850,34855,36864,36867,36868,37377,37378,37380,37386,41986,41987,42034,42036};
 for(unsigned i=0;i<sizeof allow/sizeof allow[0];i++){
  const FactoryExifTag *t=fe_find(source_exif,source_n,allow[i]);
  if(t&&(allow[i]==33437||allow[i]==37386)&&
     (t->type!=5||t->count!=1||!fe32(t->value)||!fe32(t->value+4)))continue;
  if(t)kept[n++]=*t;
 }
 fep32(w,width);fep32(h,height);
 kept[n++]=(FactoryExifTag){40961,3,1,color,2};
 kept[n++]=(FactoryExifTag){40962,4,1,w,4};kept[n++]=(FactoryExifTag){40963,4,1,h,4};
 for(unsigned i=0;i<n;i++)for(unsigned j=i+1;j<n;j++)if(kept[j].id<kept[i].id){FactoryExifTag t=kept[i];kept[i]=kept[j];kept[j]=t;}
 uint32_t end=62+2+12*n+4;
 if(capacity<10u+end)return 0;
 memset(out,0,capacity);memcpy(out+4,"Exif\0\0II\052\000",10);fep32(out+14,8);
 if(!fe_emit(out+10,capacity-10,8,root,4,&end)||!fe_emit(out+10,capacity-10,62,kept,n,&end)||end+6>65533)return 0;
 out[0]=255;out[1]=225;out[2]=(uint8_t)((end+8)>>8);out[3]=(uint8_t)(end+8);
 *length=end+10;return 1;
}
#endif
