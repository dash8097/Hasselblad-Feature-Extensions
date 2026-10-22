/* 离线真实 RAW 头输入；输出候选 APP1，不打印拍摄元数据。 */
#include <stdio.h>
#include <stdlib.h>
#include "compat_mobile_jpeg_exif.h"
int main(int argc,char **argv){
 if(argc!=3)return 2;
 uint8_t input[16384],output[65536];uint32_t length=0;
 FILE *f=fopen(argv[1],"rb");if(!f)return 3;
 size_t got=fread(input,1,sizeof input,f);fclose(f);if(got!=sizeof input)return 4;
 if(!factory_native_exif(input,sizeof input,23326,17498,output,sizeof output,&length))return 5;
 FactoryExifTag r[128],e[128],m[128];unsigned rn,en,mn;
 if(!fe_ifd(output+10,length-10,8,r,&rn))return 6;
 const FactoryExifTag *t=fe_find(r,rn,34665);if(!t)return 7;
 if(!fe_ifd(output+10,length-10,fe32(t->value),e,&en))return 8;
 t=fe_find(e,en,37500);if(!t||t->bytes!=30)return 9;
 if(!fe_ifd(output+10,length-10,(uint32_t)(t->value-output-10),m,&mn)||mn!=2)return 10;
 if(m[0].id!=0x11||m[1].id!=0x12||!fe32(m[0].value)||!fe32(m[1].value))return 11;
 if(fe_find(e,en,42033)||fe_find(e,en,42037)||fe_find(r,rn,34853))return 12;
 f=fopen(argv[2],"wbx");if(!f)return 13;
 int ok=fwrite(output,1,length,f)==length;int close_result=fclose(f);
 if(!ok||close_result)return 14;
 uint32_t sentinel=0x12345678;
 if(factory_native_exif(input,sizeof input,23326,17498,output,128,&sentinel)||sentinel!=0x12345678)return 15;
 if(factory_native_exif(input,64,23326,17498,output,sizeof output,&sentinel)||sentinel!=0x12345678)return 16;
 if(!fe_ifd(input,sizeof input,fe32(input+4),r,&rn))return 17;
 t=fe_find(r,rn,34665);if(!t||!fe_ifd(input,sizeof input,fe32(t->value),e,&en))return 18;
 if(!mobile_maker_ifd(input,sizeof input,fe32(t->value),m,&mn))return 19;
 t=fe_find(m,mn,0x12);if(!t)return 20;
 memset((void*)t->value,0,4);
 if(factory_native_exif(input,sizeof input,23326,17498,output,sizeof output,&sentinel)||sentinel!=0x12345678)return 21;
 puts("EXIF_IDENTITY_WHITELIST_AND_REJECTION_CASES_PASS");return 0;
}
