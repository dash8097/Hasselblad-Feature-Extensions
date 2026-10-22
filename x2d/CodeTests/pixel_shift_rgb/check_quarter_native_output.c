/* 用独立坐标公式核对实际分段计算，真实线程重叠，及线程失败收尾。
 * 自造图像，无照片、相机或 USB 操作。 */
#include <windows.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "quarter_native_output.h"
typedef HANDLE pthread_t;
typedef int pthread_attr_t;
typedef struct {void *(*function)(void*);void *argument;} Thread;
static volatile LONG live_threads,created_threads;
static int fail_create=-1,fail_join;
static DWORD WINAPI run_thread(void *opaque){
 Thread t=*(Thread*)opaque;free(opaque);InterlockedIncrement(&live_threads);
 t.function(t.argument);InterlockedDecrement(&live_threads);return 0;
}
static int start_thread(pthread_t *out,const pthread_attr_t *attr,void *(*fn)(void*),void *arg){
 (void)attr;LONG count=InterlockedIncrement(&created_threads)-1;
 if(count==fail_create)return 1;
 Thread *t=malloc(sizeof *t);assert(t);*t=(Thread){fn,arg};
 *out=CreateThread(NULL,0,run_thread,t,0,NULL);assert(*out);return 0;
}
static int join_thread(pthread_t thread,void **result){
 (void)result;assert(WaitForSingleObject(thread,INFINITE)==WAIT_OBJECT_0);CloseHandle(thread);return fail_join;
}
static int pause_thread(const struct timespec *a,struct timespec *b){(void)a;(void)b;Sleep(1);return 0;}
#define QN_THREAD_EXTERNAL 1
#define pthread_create start_thread
#define pthread_join join_thread
#define nanosleep pause_thread
#include "quarter_native_output.c"
#undef pthread_create
#undef pthread_join
#undef nanosleep
#include "factory_native_3fr.h"
static void put_ifd(uint8_t *h,unsigned offset,const uint32_t tags[][4],unsigned count){
 f3_p16(h+offset,(uint16_t)count);
 for(unsigned i=0;i<count;i++){
  uint8_t *p=h+offset+2+12*i;f3_p16(p,(uint16_t)tags[i][0]);f3_p16(p+2,(uint16_t)tags[i][1]);
  f3_p32(p+4,tags[i][2]);f3_p32(p+8,tags[i][3]);
 }
}
typedef struct {QnRawInput in;QnRawOutput out;int result;} Conversion;
static uint16_t value(unsigned x,unsigned y){return (uint16_t)(x*811+y*131+17000*(x%2)+31000*(y%2));}
static void *convert_job(void *opaque){Conversion *job=opaque;job->result=qn_raw_convert(&job->in,&job->out,4);return NULL;}
static void check(const Conversion *c){
 for(unsigned y=0;y<c->out.height;y++)for(unsigned x=0;x<c->out.width;x++){
  unsigned sy=4*(y/2)+y%2,sx=4*(x/2)+x%2,by=sy+2<c->in.height?sy+2:sy;
  uint16_t expected=(uint16_t)(((uint32_t)value(sx,sy)+value(sx+2,sy)+value(sx,by)+value(sx+2,by)+2)/4);
  assert(c->out.pixels[(size_t)y*c->out.stride+x]==expected);
 }
 for(unsigned y=0;y<c->out.height;y++)for(unsigned x=c->out.width;x<c->out.stride;x++)assert(!c->out.pixels[(size_t)y*c->out.stride+x]);
}
typedef struct {
 int fail_allocate,fail_commit,bad_lease,partial_allocate,forget_move;
 size_t storage_extra;
 unsigned allocated,released,committed;
 QnRawPlan plan;QnOutputLease installed;
} Writer;
static int allocate_output(void *opaque,size_t bytes,QnOutputLease *lease){
 Writer *w=opaque;w->allocated++;
 if(w->fail_allocate&&!w->partial_allocate)return 0;
 bytes+=w->storage_extra;void *data=malloc(bytes);assert(data);memset(data,0xa5,bytes);
 *lease=(QnOutputLease){.output={data,bytes,5952,4421,5952},.native_owner=data};
 if(w->bad_lease)lease->output.height--;
 return !w->fail_allocate;
}
static void release_output(void *opaque,QnOutputLease *lease){
 Writer *w=opaque;assert(!live_threads);w->released++;
 free(lease->native_owner);memset(lease,0,sizeof *lease);
}
static int commit_output(void *opaque,const QnRawPlan *plan,QnOutputLease *lease){
 Writer *w=opaque;assert(!live_threads);w->committed++;
 if(w->fail_commit)return 0;
 if(w->forget_move)return 1;
 w->plan=*plan;w->installed=*lease;memset(lease,0,sizeof *lease);return 1;
}
static unsigned check_writer(const uint8_t *header){
 unsigned cases=0;uint16_t *source=malloc(210510336);assert(source);
 for(unsigned y=0;y<8842;y++)for(unsigned x=0;x<11904;x++)source[(size_t)y*11904+x]=value(x,y);
 RsPixelBlock segment={source,210510336};QnRawInput input={11904,8842,11904,1,&segment};
 /* 故障不修改原始输入，也不发布半成品写入工作。 */
 for(unsigned fault=0;fault<6;fault++){
  Writer w={.fail_allocate=fault==0||fault==1,.partial_allocate=fault==1,
   .bad_lease=fault==2,.fail_commit=fault==3,.forget_move=fault==4};
  QnWriteAdapter api={.context=&w,.allocate=allocate_output,.release=release_output,.commit=commit_output};
  assert(qn_raw_work_prepare(&input,header,47,api)==(fault==5));
  assert(w.allocated==1&&w.released==(fault!=0&&fault!=5));
  assert(w.committed==(fault>=3));assert(!live_threads);
  if(fault==5){
   Conversion c={.in=input,.out=w.installed.output};check(&c);
   assert(w.plan.total_bytes%4096==0&&w.plan.preview_bytes==47);
   assert(w.installed.native_owner&&w.installed.output.pixels);
   release_output(&w,&w.installed);
  }
  cases++;
 }
 for(size_t extra=1;extra<4096;extra+=1919){
  Writer w={.storage_extra=extra};
  QnWriteAdapter api={.context=&w,.allocate=allocate_output,.release=release_output,.commit=commit_output,
   .preview_storage_bytes=4096};
  assert(qn_raw_work_prepare(&input,header,47,api));
  assert(w.plan.raw_storage_bytes==52627584+extra&&w.plan.preview_offset==52643968+extra);
  assert(w.plan.preview_bytes==47&&w.plan.preview_storage_bytes==4096&&w.plan.total_bytes%4096==0);
  assert(f3_is(w.plan.header,16384,8,273,(uint32_t)w.plan.preview_offset));
  for(size_t i=52627584;i<w.installed.output.bytes;i++)assert(!((uint8_t*)w.installed.output.pixels)[i]);
  release_output(&w,&w.installed);cases++;
 }
 Writer w={0};QnWriteAdapter api={.context=&w,.allocate=allocate_output,.release=release_output,.commit=commit_output};
 fail_join=1;assert(!qn_raw_work_prepare(&input,header,47,api));fail_join=0;
 assert(w.allocated==1&&w.released==1&&!w.committed&&!live_threads);cases++;
 input.width=11656;assert(!qn_raw_work_prepare(&input,header,47,api));
 assert(w.allocated==1&&w.released==1&&!w.committed);cases++;
 for(unsigned y=0;y<8842;y++)for(unsigned x=0;x<11904;x++)assert(source[(size_t)y*11904+x]==value(x,y));
 free(source);return cases;
}
int main(void){
 unsigned cases=0;
 for(unsigned height=10;height<=8842;height+=(height==10?8832:1)){
  unsigned width=height==10?28:11904,stride=width+4,rows=height;
  uint16_t *source=malloc((size_t)stride*rows*2);assert(source);
  for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<stride;x++)source[(size_t)y*stride+x]=value(x,y);
  RsPixelBlock segments[8];
  for(unsigned i=0;i<8;i++){
   unsigned first=(uint64_t)rows*i/8,end=(uint64_t)rows*(i+1)/8;
   segments[i]=(RsPixelBlock){source+(size_t)first*stride,(size_t)(end-first)*stride*2};
  }
  Conversion jobs[2];
  for(unsigned i=0;i<2;i++)jobs[i]=(Conversion){.in={width,rows,stride,8,segments},
   .out={calloc((size_t)(width/2+4)*(rows/2),2),(size_t)(width/2+4)*(rows/2)*2,width/2,rows/2,width/2+4}};
  assert(jobs[0].out.pixels&&jobs[1].out.pixels);
  pthread_t thread;assert(!start_thread(&thread,NULL,convert_job,jobs));
  convert_job(jobs+1);assert(!join_thread(thread,NULL));assert(jobs[0].result&&jobs[1].result);
  check(jobs);check(jobs+1);cases+=2;
  /* 输出或源段越界时，整个目标保持原值。 */
  uint8_t *before=malloc(jobs[0].out.bytes);assert(before);memcpy(before,jobs[0].out.pixels,jobs[0].out.bytes);
  size_t original=segments[3].bytes;segments[3].bytes--;
  assert(!qn_raw_convert(&jobs[0].in,&jobs[0].out,4));assert(!memcmp(before,jobs[0].out.pixels,jobs[0].out.bytes));segments[3].bytes=original;cases++;
  QnRawOutput alias=jobs[0].out;alias.pixels=source;
  assert(!qn_raw_convert(&jobs[0].in,&alias,4));cases++;
  /* 线程创建失败由当前工人完成剩余分块，不丢行或依赖拍摄任务锁。 */
  for(int i=0;i<3;i++){
   created_threads=0;fail_create=i;assert(qn_raw_convert(&jobs[0].in,&jobs[0].out,4));check(jobs);assert(!live_threads);cases++;
  }
  fail_create=-1;fail_join=1;assert(!qn_raw_convert(&jobs[0].in,&jobs[0].out,4));assert(!live_threads);fail_join=0;cases++;
  /* 各段独立分配且从行中断开，不能依赖相邻地址或整行边界。 */
  size_t pixels=(size_t)stride*rows,used=0;
  RsPixelBlock broken[8];
  for(unsigned i=0;i<8;i++){
   size_t end=i==7?pixels:(pixels*(i+1)/8+(i%3));
   size_t count=end-used;uint16_t *piece=malloc(count*2);assert(piece);
   memcpy(piece,source+used,count*2);broken[i]=(RsPixelBlock){piece,count*2};used=end;
  }
  jobs[0].in.blocks=broken;
  assert(qn_raw_convert(&jobs[0].in,&jobs[0].out,4));check(jobs);cases++;
  for(unsigned i=0;i<8;i++)free((void*)broken[i].pixels);
  free(before);free(jobs[0].out.pixels);free(jobs[1].out.pixels);free(source);
 }
 QnExposureOutput first,second;
 assert(qn_exposure_output(&first,QN_QUARTER_SIZE,11656,8742));
 assert(qn_exposure_output(&second,QN_NATIVE_SIZE,11656,8742));
 assert(first.resolution==25&&first.effective_width==5828&&first.effective_height==4371);
 assert(second.resolution==100&&second.effective_width==11656&&second.effective_height==8742);cases++;
 assert(!qn_exposure_output(&second,QN_QUARTER_SIZE,11655,8742));cases++;
 uint8_t header[16384]={0};memcpy(header,"II*\0",4);f3_p32(header+4,8);
 const uint32_t main_tags[][4]={{330,4,1,256},{273,4,1,210526720},{279,4,1,47}};
 const uint32_t raw_tags[][4]={{256,4,1,11904},{257,4,1,8842},{258,3,1,16},{259,3,1,1},
  {273,4,1,16384},{278,4,1,8842},{279,4,1,210510336},{50719,4,2,600},{50720,4,2,608}};
 put_ifd(header,8,main_tags,3);put_ifd(header,256,raw_tags,9);
 f3_p32(header+600,124);f3_p32(header+604,92);f3_p32(header+608,11656);f3_p32(header+612,8742);
 QnRawPlan plan;
 for(unsigned preview=47;preview<9000;preview+=311){
  assert(qn_raw_plan(header,preview,&plan));assert(plan.raw_bytes==5952ull*4421*2);
  assert(plan.preview_offset==16384+plan.raw_bytes&&plan.total_bytes%4096==0);
  assert(plan.total_bytes-plan.preview_offset-preview==plan.padding&&plan.padding<4096);
  assert(f3_is(plan.header,16384,256,256,5952)&&f3_is(plan.header,16384,256,257,4421));
  assert(f3_is(plan.header,16384,8,273,52643968)&&f3_is(plan.header,16384,8,279,preview));
  assert(f3_is(header,16384,256,256,11904));cases++;
 }
 cases+=check_writer(header);
 QnRawPlan unchanged=plan;header[0]=0;
 assert(!qn_raw_plan(header,47,&plan)&&!memcmp(&unchanged,&plan,sizeof plan));cases++;
 printf("QUARTER_NATIVE_BACKGROUND_PARALLEL_SEGMENTS_OWNERSHIP_AND_MODE_SNAPSHOT_PASSED cases=%u\n",cases);return 0;
}
