#ifndef PIXEL_SHUTTER_INTENT_H
#define PIXEL_SHUTTER_INTENT_H
/* 待拍请求绑定本次 arm 文件的 inode 和纳秒修改时间。取消后重新应用
 * 即使参数相同，也不能兑现旧请求；不使用图像或设备身份信息。 */
static inline int po_intent_write(char *out,size_t capacity,const char *arm,const uint64_t generation[3]){
 int n=snprintf(out,capacity,"POQ1 %llu %llu %llu\n%s",
  (unsigned long long)generation[0],(unsigned long long)generation[1],
  (unsigned long long)generation[2],arm);
 return n>0&&(size_t)n<capacity?n:0;
}
static inline int po_intent_read(const char *input,size_t length,const uint64_t generation[3],char out[129]){
 unsigned long long a=0,b=0,c=0;int used=0;
 if(!input||length<20||length>=256||input[length]!=0||
    sscanf(input,"POQ1 %llu %llu %llu\n%n",&a,&b,&c,&used)!=3||used<=0||
    (size_t)used>=length||a!=generation[0]||b!=generation[1]||c!=generation[2]||c>=1000000000)return 0;
 size_t n=length-(size_t)used;if(n<25||n>127||input[length-1]!='\n')return 0;
 char exact[256];memcpy(out,input+used,n+1);
 int written=po_intent_write(exact,sizeof exact,out,generation);
 return written==(int)length&&!memcmp(exact,input,length)?(int)n:0;
}
#endif
