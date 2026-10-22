/* 逐块进度，只记录阶段和完成数量；不含图像、句柄或设备信息。 */
#ifndef X2D_SIXTEEN_PROGRESS_H
#define X2D_SIXTEEN_PROGRESS_H
static inline unsigned sixteen_progress_line(char out[32],unsigned phase,unsigned rendered,unsigned encoded){
 if(phase<1||phase>3||rendered>25||encoded>rendered||rendered-encoded>1)return 0;
 unsigned values[3]={phase,rendered,encoded},n=0;
 const char *prefix="RS16 ";for(unsigned i=0;prefix[i];i++)out[n++]=prefix[i];
 for(unsigned i=0;i<3;i++){
  if(values[i]>=10)out[n++]=(char)('0'+values[i]/10);
  out[n++]=(char)('0'+values[i]%10);out[n++]=i==2?'\n':' ';
 }
 out[n]=0;return n;
}
static inline int sixteen_progress_parse(const char *s,unsigned *phase,unsigned *rendered,unsigned *encoded){
 const char *prefix="RS16 ";for(unsigned i=0;prefix[i];i++)if(*s++!=prefix[i])return 0;
 unsigned v[3]={0};
 for(unsigned i=0;i<3;i++){
  unsigned digits=0;while(*s>='0'&&*s<='9'){if(++digits>2)return 0;v[i]=v[i]*10+(unsigned)(*s++-'0');}
  if(!digits||*s++!=(i==2?'\n':' '))return 0;
 }
 if(*s||v[0]<1||v[0]>3||v[1]>25||v[2]>v[1]||v[1]-v[2]>1)return 0;
 *phase=v[0];*rendered=v[1];*encoded=v[2];return 1;
}
/* 拼接日志只消费完整控制行；尾行未写完、错误或倒退均不推定完成。 */
static inline int sixteen_join_progress_parse(const char *s,unsigned *rows,unsigned *total,int *complete){
 unsigned last=0;int seen=0,done=0;
 while(*s){
  const char *line=s,*end=s;while(*end&&*end!='\n')end++;
  if(*end!='\n')break;
  const char *prefix="JPEG_GRID_COMPRESSED_ROWS ",*p=line,*q=prefix;
  while(*q&&p<end&&*p==*q){p++;q++;}
  if(!*q){
   unsigned values[2]={0};int valid=1;
   for(unsigned i=0;i<2;i++){
    unsigned digits=0;while(p<end&&*p>='0'&&*p<='9'){
     if(++digits>5){valid=0;break;}values[i]=values[i]*10+(unsigned)(*p++-'0');
    }
    if(!valid||!digits||(i==0&&(p==end||*p++!='/'))){valid=0;break;}
   }
   if(!valid||p!=end||values[1]!=34996||values[0]>values[1]||values[0]<last)return 0;
   last=values[0];seen=1;
  }else{
   const char *finish="JPEG_GRID_NATIVE_TABLES_NO_IDCT_NO_REQUANTIZE_COMPLETE";
   p=line;q=finish;while(*q&&p<end&&*p==*q){p++;q++;}
   if(!*q&&p==end)done=last==34996;
   const char *failed="JPEG_GRID_FAILED_INPUTS_RETAINED";
   p=line;q=failed;while(*q&&p<end&&*p==*q){p++;q++;}
   if(!*q&&p==end)return 0;
  }
  s=end+1;
 }
 if(!seen)return 0;*rows=last;*total=34996;*complete=done;return 1;
}
#endif
