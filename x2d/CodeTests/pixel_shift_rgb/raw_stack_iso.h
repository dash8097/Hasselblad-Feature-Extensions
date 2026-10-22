/* ISO 平均堆栈：只处理帧级曝光参数及最终自有 TIFF 头。 */
#ifndef RAW_STACK_ISO_H
#define RAW_STACK_ISO_H
#include "factory_native_3fr.h"
#include <stdio.h>
typedef struct {uint32_t iso,numerator,denominator;} RsIsoExposure;
typedef struct {RsIsoExposure source;uint32_t target,frames,planned,iso,numerator,denominator;} RsIsoResult;
static inline uint64_t rs_iso_gcd(uint64_t a,uint64_t b){while(b){uint64_t t=a%b;a=b;b=t;}return a;}
static inline int rs_iso_exposure(const uint8_t header[F3_HEADER],RsIsoExposure *out){
 if(!header||!out||memcmp(header,"II*\0",4))return 0;
 uint8_t h[F3_HEADER];memcpy(h,header,sizeof h);uint32_t exif;
 if(!f3_scalar(h,sizeof h,f3_u32(h+4),34665,&exif))return 0;
 uint8_t *iso=f3_data(h,sizeof h,f3_tag(h,sizeof h,exif,34855),3,1,2);
 uint8_t *time=f3_data(h,sizeof h,f3_tag(h,sizeof h,exif,33434),5,1,8);
 if(!iso||!time||!f3_u16(iso)||!f3_u32(time)||!f3_u32(time+4))return 0;
 uint32_t n=f3_u32(time),d=f3_u32(time+4);uint64_t g=rs_iso_gcd(n,d);
 *out=(RsIsoExposure){f3_u16(iso),(uint32_t)(n/g),(uint32_t)(d/g)};return 1;
}
static inline int rs_iso_same(RsIsoExposure a,RsIsoExposure b){
 return a.iso==b.iso&&a.numerator&&a.denominator&&b.numerator&&b.denominator&&
  (uint64_t)a.numerator*b.denominator==(uint64_t)b.numerator*a.denominator;
}
static inline uint32_t rs_iso_plan(uint32_t source,uint32_t target){
 return source&&source<=65535&&target&&target<=source?(source+target-1)/target:0;
}
static inline int rs_iso_result(RsIsoExposure source,uint32_t target,uint32_t frames,RsIsoResult *out){
 uint32_t planned=rs_iso_plan(source.iso,target);
 if(!out||!planned||!frames||frames>planned||!source.numerator||!source.denominator)return 0;
 uint64_t n=(uint64_t)source.numerator*frames,d=source.denominator,g=rs_iso_gcd(n,d);n/=g;d/=g;
 if(n>UINT32_MAX||d>UINT32_MAX)return 0;
 /* EXIF ISO 是整数；保留 source/frames 供界面与记录显示精确比值。 */
 uint32_t iso=(source.iso+frames/2)/frames;
 if(!iso||iso>65535)return 0;
 *out=(RsIsoResult){source,target,frames,planned,iso,(uint32_t)n,(uint32_t)d};return 1;
}
static inline int rs_iso_record(const char *text,RsIsoResult *out){
 unsigned source,target,n,d,frames,planned;int used=0;char canonical[160];RsIsoResult result;
 if(!text||sscanf(text,"RSI1 %u %u %u %u %u %u\n%n",&source,&target,&n,&d,&frames,&planned,&used)!=6||text[used]||
    !rs_iso_result((RsIsoExposure){source,n,d},target,frames,&result)||result.planned!=planned)return 0;
 int length=snprintf(canonical,sizeof canonical,"RSI1 %u %u %u %u %u %u\n",source,target,n,d,frames,planned);
 if(length<=0||strcmp(canonical,text))return 0;*out=result;return 1;
}
static inline int rs_iso_stamp(const uint8_t source[F3_HEADER],const RsIsoResult *result,uint8_t out[F3_HEADER]){
 RsIsoExposure actual;RsIsoResult checked;uint8_t h[F3_HEADER],time[8];uint32_t exif;
 if(!source||!out||!result||!rs_iso_exposure(source,&actual)||!rs_iso_same(actual,result->source)||
    !rs_iso_result(result->source,result->target,result->frames,&checked)||checked.planned!=result->planned)return 0;
 memcpy(h,source,sizeof h);
 if(!f3_scalar(h,sizeof h,f3_u32(h+4),34665,&exif))return 0;
 f3_p32(time,checked.numerator);f3_p32(time+4,checked.denominator);
 if(!f3_put32(h,exif,34855,checked.iso)||!f3_put(h,exif,33434,5,1,time,8)||!f3_omit(h,exif,37377))return 0;
 memcpy(out,h,sizeof h);return 1;
}
#endif
