/* 自造控制日志：完整行、单调性、写盘完成与失败不混淆。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "sixteen_progress.h"
int main(void){
 unsigned rows=0,total=0;int done=0;unsigned cases=0;
 const char *valid[]={
  "JPEG_GRID_COMPRESSED_ROWS 1024/34996\n",
  "JPEG_GRID_COMPRESSED_ROWS 1024/34996\nJPEG_GRID_COMPRESSED_ROWS 17498/34996\npartial",
  "JPEG_GRID_COMPRESSED_ROWS 34996/34996\n",
  "JPEG_GRID_COMPRESSED_ROWS 34996/34996\nJPEG_GRID_NATIVE_TABLES_NO_IDCT_NO_REQUANTIZE_COMPLETE\n"};
 const unsigned expected[]={1024,17498,34996,34996};
 for(unsigned i=0;i<4;i++){
  assert(sixteen_join_progress_parse(valid[i],&rows,&total,&done));
  assert(rows==expected[i]&&total==34996&&done==(i==3));cases++;
 }
 const char *invalid[]={
  "JPEG_GRID_COMPRESSED_ROWS 1024/34996", "JPEG_GRID_COMPRESSED_ROWS 34997/34996\n",
  "JPEG_GRID_COMPRESSED_ROWS 1/34995\n", "JPEG_GRID_COMPRESSED_ROWS 999999999999/34996\n",
  "JPEG_GRID_COMPRESSED_ROWS -1/34996\n", "JPEG_GRID_COMPRESSED_ROWS 1/34996 junk\n",
  "JPEG_GRID_COMPRESSED_ROWS 2000/34996\nJPEG_GRID_COMPRESSED_ROWS 1000/34996\n",
  "JPEG_GRID_COMPRESSED_ROWS 34996/34996\nJPEG_GRID_FAILED_INPUTS_RETAINED\n"};
 for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){
  rows=123;total=456;done=0;assert(!sixteen_join_progress_parse(invalid[i],&rows,&total,&done));
  assert(rows==123&&total==456&&!done);cases++;
 }
 assert(sixteen_join_progress_parse("JPEG_GRID_COMPRESSED_ROWS 1024/34996\nJPEG_GRID_COMPRESSED_ROWS 2048/349",&rows,&total,&done));
 assert(rows==1024&&!done);cases++;
 printf("SIXTEEN_JOIN_PROGRESS_CONTROL_LOG_PASSED cases=%u\n",cases);return 0;
}
