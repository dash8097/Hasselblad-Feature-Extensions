/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hasselblad Feature Extensions contributors
 */
/* 合成内存替身；可写出小幅测试结果供冻结旧版逐字节对照。
 * 内存合成函数本身没有文件接口。 */
#include <stdio.h>
#include <stdlib.h>
#include "capture_memory_merge.h"
#include "capture_memory_pipeline.h"
#ifdef _WIN32
#include <windows.h>
#endif
#define WIDTH 67u
#define HEIGHT 2059u
#define CHECK(v) do{if(!(v)){fprintf(stderr,"MEMORY_MERGE_CHECK_FAILED line=%u\n",__LINE__);exit(1);}}while(0)
typedef struct {unsigned begin,end;void(*run)(unsigned,unsigned,void*);void *context;} Range;
#ifdef _WIN32
static DWORD WINAPI range_thread(void *p){Range *r=p;r->run(r->begin,r->end,r->context);return 0;}
#endif
static void dispatch(void *unused,unsigned width,void(*run)(unsigned,unsigned,void*),void *row){
 (void)unused;Range ranges[4];
#ifdef _WIN32
 HANDLE threads[4];
#endif
 for(unsigned i=0;i<4;i++){
  ranges[i]=(Range){width*i/4,width*(i+1)/4,run,row};
#ifdef _WIN32
  threads[i]=CreateThread(NULL,0,range_thread,ranges+i,0,NULL);CHECK(threads[i]);
#else
  ranges[i].run(ranges[i].begin,ranges[i].end,ranges[i].context);
#endif
 }
#ifdef _WIN32
 for(unsigned i=0;i<4;i++)CHECK(WaitForSingleObject(threads[i],INFINITE)==WAIT_OBJECT_0&&CloseHandle(threads[i]));
#endif
}
typedef struct {unsigned calls,ok;} SourceFinish;
static void source_finish(void *context,int ok){SourceFinish *f=context;f->calls++;f->ok=(unsigned)ok;}
static void pipeline_check(CmView frames[6],uint16_t *golden,uint16_t *actual,void *scratch,size_t scratch_bytes){
 CmpBuffer buffers[PC_SLOTS];
 for(unsigned i=0;i<PC_SLOTS;i++)buffers[i]=(CmpBuffer){malloc(WIDTH*127*8),WIDTH*127*8,0};
 for(unsigned i=0;i<PC_SLOTS;i++)CHECK(buffers[i].pixels);
 CmPipeline p;SourceFinish finish={0};PcChunk raw,render;
 CmpBuffer overlap[PC_SLOTS];memcpy(overlap,buffers,sizeof overlap);
 overlap[1]=overlap[0];
 CHECK(!cmp_init(&p,77,frames,WIDTH,HEIGHT,127,overlap,scratch,scratch_bytes,NULL,NULL,source_finish,&finish));
 overlap[0]=(CmpBuffer){(uint16_t*)frames[0].data,WIDTH*127*8,0};
 CHECK(!cmp_init(&p,77,frames,WIDTH,HEIGHT,127,overlap,scratch,scratch_bytes,NULL,NULL,source_finish,&finish));
 CHECK(finish.calls==0);
 CHECK(cmp_init(&p,77,frames,WIDTH,HEIGHT,127,buffers,scratch,scratch_bytes,NULL,NULL,source_finish,&finish));
 for(unsigned i=0;i<PC_SLOTS;i++)CHECK(cmp_step(&p)==CMP_SUBMITTED);
 CHECK(p.next_row<HEIGHT&&cmp_step(&p)==CMP_WAIT&&finish.calls==0);
 CHECK(cmp_take(&p,PC_RAW,&raw)&&cmp_take(&p,PC_RENDER,&render));
 CHECK(raw.pixels==render.pixels&&raw.first_row==0&&raw.bytes==WIDTH*127*8);
 memcpy(actual,raw.pixels,raw.bytes);
 CHECK(cmp_done(&p,PC_RAW,raw.task,raw.ticket,1));
 CHECK(cmp_step(&p)==CMP_WAIT); /* 显影尚在使用同一个块，写入完成不能释放。 */
 CHECK(!memcmp(golden,render.pixels,render.bytes));
 CHECK(cmp_done(&p,PC_RENDER,render.task,render.ticket,1));
 CHECK(cmp_step(&p)==CMP_SUBMITTED&&p.next_row<HEIGHT);
 while(!cmp_complete(&p)){
  if(cmp_take(&p,PC_RAW,&raw)){
   memcpy(actual+(size_t)raw.first_row*WIDTH*2,raw.pixels,raw.bytes);
   CHECK(cmp_done(&p,PC_RAW,raw.task,raw.ticket,1));
  }
  if(cmp_take(&p,PC_RENDER,&render)){
   CHECK(!memcmp(golden+(size_t)render.first_row*WIDTH*2,render.pixels,render.bytes));
   CHECK(cmp_done(&p,PC_RENDER,render.task,render.ticket,1));
  }
  int state=cmp_step(&p);CHECK(state!=CMP_FAILED);
  if(state==CMP_MERGED)CHECK(finish.calls==1&&finish.ok==1);
 }
 CHECK(!memcmp(golden,actual,(size_t)WIDTH*HEIGHT*8)&&finish.calls==1);
 /* 写入失败：未交接的块回收，仍在显影的块等真实完成；源引用只结束一次。 */
 finish=(SourceFinish){0};
 CHECK(cmp_init(&p,78,frames,WIDTH,HEIGHT,127,buffers,scratch,scratch_bytes,NULL,NULL,source_finish,&finish));
 CHECK(cmp_step(&p)==CMP_SUBMITTED&&cmp_step(&p)==CMP_SUBMITTED);
 CHECK(cmp_take(&p,PC_RAW,&raw)&&cmp_take(&p,PC_RENDER,&render));
 CHECK(cmp_done(&p,PC_RAW,raw.task,raw.ticket,0)&&finish.calls==1&&!finish.ok);
 CHECK(!pc_drained(&p.output)&&cmp_step(&p)==CMP_FAILED);
 CHECK(cmp_done(&p,PC_RENDER,render.task,render.ticket,1)&&pc_drained(&p.output));
 cmp_abort(&p);CHECK(finish.calls==1&&!cmp_complete(&p));
 for(unsigned i=0;i<PC_SLOTS;i++)free(buffers[i].pixels);
}
int main(int argc,char **argv){
 CHECK(argc==2);CmView frames[6];uint16_t *planes[6];
 for(unsigned i=0;i<6;i++){
  planes[i]=calloc(1,210510336);CHECK(planes[i]);
  for(unsigned y=92;y<93+HEIGHT;y++)for(unsigned x=124;x<126+WIDTH;x++)
   planes[i][(size_t)y*11904+x]=(uint16_t)((x*251+y*17+i*10007)&65535);
  frames[i]=(CmView){11836,8842,23808,1,(const uint8_t*)planes[i],210510336,0};
 }
 size_t bytes=(size_t)WIDTH*HEIGHT*8,scratch_bytes=cm_merge_scratch_bytes(WIDTH);
 uint16_t *output=malloc(bytes),*other=malloc(bytes);void *scratch=malloc(scratch_bytes);
 CHECK(output&&other&&scratch);
 unsigned batches[3]={1,127,1024};
 for(unsigned test=0;test<3;test++){
 for(unsigned start=0;start<HEIGHT;start+=batches[test]){
  unsigned rows=HEIGHT-start<batches[test]?HEIGHT-start:batches[test];
  CHECK(cm_merge_rows(frames,WIDTH,HEIGHT,start,rows,(test?other:output)+(size_t)start*WIDTH*4,
   (size_t)WIDTH*rows*8,scratch,scratch_bytes,NULL,NULL));
 }
 if(test)CHECK(!memcmp(output,other,bytes));
 }
 /* 各个批次边界与整体相同，前一批/后一批所需周边行不能丢失。 */
 CHECK(!memcmp(output,other,bytes));
 CHECK(cm_merge_rows(frames,WIDTH,HEIGHT,1017,17,other,(size_t)WIDTH*17*8,scratch,scratch_bytes,dispatch,NULL));
 CHECK(!memcmp(output+(size_t)1017*WIDTH*4,other,(size_t)WIDTH*17*8));
 CmView bad[6];memcpy(bad,frames,sizeof bad);bad[4].stride--;
 memset(other,0x55,128);
 CHECK(!cm_merge_rows(bad,WIDTH,HEIGHT,0,1,other,WIDTH*8,scratch,scratch_bytes,NULL,NULL));
 for(unsigned i=0;i<64;i++)CHECK(other[i]==0x5555);
 CHECK(!cm_merge_rows(frames,WIDTH,HEIGHT,HEIGHT,1,other,WIDTH*8,scratch,scratch_bytes,NULL,NULL));
 CHECK(!cm_merge_rows(frames,WIDTH,HEIGHT,0,1025,other,WIDTH*1025*8,scratch,scratch_bytes,NULL,NULL));
 CHECK(!cm_merge_rows(frames,WIDTH,HEIGHT,0,1,other,WIDTH*8,scratch,scratch_bytes-1,NULL,NULL));
 pipeline_check(frames,output,other,scratch,scratch_bytes);
 FILE *f=fopen(argv[1],"wb");CHECK(f);CHECK(fwrite(output,1,bytes,f)==bytes&&!fclose(f));
 for(unsigned i=0;i<6;i++)free(planes[i]);free(output);free(other);free(scratch);
 puts("MEMORY_SIX_KERNEL_BATCH_HALO_AND_COMPUTE_PARTITION_CHECKS_PASSED_OFFLINE");return 0;
}
