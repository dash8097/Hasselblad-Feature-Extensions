/* 运行时只允许本项目独占 UI 任务目录，输出路径不由界面自由指定。 */
#ifndef FACTORY_RUNTIME_JOB_H
#define FACTORY_RUNTIME_JOB_H
static int factory_path_equal(const char *a,const char *b){
 while(*a&&*a==*b){++a;++b;}return *a==*b;
}
static int factory_runtime_paths(const char *raw,const char *ready,char *main_out,char *preview_out){
 const char *prefix="/mnt/media_rw/ssd/multishot/native-album-ui-";
 unsigned n=0;
 while(prefix[n]){if(!raw||raw[n]!=prefix[n])return 0;++n;}
 unsigned start=n;
 while(raw[n]&&raw[n]!='/'){
  char c=raw[n];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-')||n-start>=30)return 0;
  ++n;
 }
#if defined(FACTORY_NATIVE_DISK400)
 if(n==start||!factory_path_equal(raw+n,"/merged.dng"))return 0;
#elif defined(FACTORY_MEMORY_BANDS)
 if(n==start||!factory_path_equal(raw+n,"/memory.sock"))return 0;
#elif defined(FACTORY_PACKED_DNG_INPUT)
 if(n==start||!factory_path_equal(raw+n,"/merged.dng"))return 0;
#else
 if(n==start||!factory_path_equal(raw+n,"/input.raw"))return 0;
#endif
 for(unsigned i=0;i<n;++i)if(!ready||ready[i]!=raw[i])return 0;
 if(!factory_path_equal(ready+n,"/config.ready"))return 0;
 const char *suffixes[]={"/render.jpg","/preview.jpg"};char *outputs[]={main_out,preview_out};
 for(unsigned k=0;k<2;++k){
  for(unsigned i=0;i<n;++i)outputs[k][i]=raw[i];
  unsigned i=0;do{outputs[k][n+i]=suffixes[k][i];}while(suffixes[k][i++]);
 }
 return 1;
}
#endif
