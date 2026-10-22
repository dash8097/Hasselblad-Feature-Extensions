/* 执行生产底边读块函数；只读自造RAW，不接相机。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define main production_merge_main
#include "sixteen_disk_merge.c"
#undef main
#include <assert.h>
int main(void){
 FILE *file=tmpfile();assert(file);
 uint16_t *row=malloc(23808),*out=malloc(3*23808);assert(row&&out);
 for(unsigned y=8839;y<8842;y++){
  for(unsigned x=0;x<11904;x++)row[x]=(uint16_t)(100+y%4*1000+x%97);
  assert(seek_at(file,16384+(uint64_t)y*23808));assert(fwrite(row,2,11904,file)==11904);
 }
 assert(fflush(file)==0);
 assert(read_source_rows(file,out,8840,3));
 for(unsigned y=0;y<3;y++)for(unsigned x=0;x<11904;x+=173){
  unsigned actual=y==1?8841:8840;
  assert(out[y*11904+x]==100+actual%4*1000+x%97);
 }
 assert(read_source_rows(file,out,8842,1));
 for(unsigned x=0;x<11904;x+=173)assert(out[x]==100+8840%4*1000+x%97);
 assert(!read_source_rows(file,out,8843,1));assert(!read_source_rows(file,out,8842,0));
 assert((uint64_t)SIXTEEN_HEIGHT+93<=8842);
 const char *args[]={"merge","unused-output","missing-self-made-job","1024","512"};
 assert(production_merge_main(5,(char**)args)==1); /* 默认尺寸不再误报参数错误2。 */
 free(row);free(out);assert(fclose(file)==0);
 puts("SIXTEEN_PRODUCTION_GEOMETRY_AND_BOTTOM_SAME_CFA_EXTENSION_PASSED");return 0;
}
