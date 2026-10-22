/* 合成输入的完整 session；冻结旧算法为外部基准，消费者确实运行在两个线程。 */
#include <stdio.h>
#include <stdlib.h>
#include "capture_memory_session.h"
#ifdef _WIN32
#include <windows.h>
static unsigned long thread_id(void){return GetCurrentThreadId();}
static void pause_poll(void){Sleep(1);}
#else
#include <pthread.h>
#include <time.h>
static unsigned long thread_id(void){return (unsigned long)pthread_self();}
static void pause_poll(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
#endif
#define W 67u
#define H 2059u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"SESSION_CHECK_FAILED line=%u\n",__LINE__);exit(1);}}while(0)
typedef struct {CmView view;unsigned held,retains,releases;} Image;
typedef struct {
 unsigned long owner;
 unsigned mode,begin_count,finish_count,abort_count,calls[2],entered[2],gate,not_owner,source_release_observed;
 Image images[7];
 const uint8_t *golden;
 FILE *file;
 uint16_t *full_result;
 unsigned full,source_gate;
} Mock;
static uint8_t jpg[]={255,216,255,192,0,17,8,11,102,15,48,3,1,0x21,0,2,0x11,0,3,0x11,0,
 255,218,0,12,3,1,0,2,0,3,0,0,63,0,42,255,0,42,255,217};
static uint16_t *planes[6];
static int allowed(void *context){Mock *m=context;return !m->not_owner&&m->owner==thread_id();}
static int retain(void *context,const X2dQtShared *from,X2dQtShared *out){
 CHECK(allowed(context));Image *i=from->value;CHECK(i&&from->control==i);i->held++;i->retains++;*out=*from;return 1;
}
static void release(void *context,X2dQtShared *from){
 CHECK(allowed(context));Image *i=from->value;CHECK(i&&from->control==i&&i->held);
 i->held--;i->releases++;memset(from,0,sizeof *from);
}
static int view(void *context,const X2dQtShared *from,CmView *out){
 CHECK(allowed(context));Image *i=from->value;CHECK(i->held);*out=i->view;return 1;
}
static int consume(Mock *m,const PcChunk *c,unsigned consumer){
 CHECK(thread_id()!=m->owner&&c->task==91);CmpBuffer *b=c->owner;CHECK(b->busy);
 if(m->full){CHECK(m->source_release_observed&&m->source_gate);
  for(unsigned i=0;i<6;i++)CHECK(!m->images[i].held);
 }
 __atomic_store_n(m->entered+consumer,1,__ATOMIC_RELEASE);
 if(m->mode==5)while(!__atomic_load_n(&m->gate,__ATOMIC_ACQUIRE))pause_poll();
 m->calls[consumer]++;
 if((m->mode==1&&consumer==PC_RAW)||(m->mode==2&&consumer==PC_RENDER))return 0;
 CHECK(!memcmp(c->pixels,m->golden+(size_t)c->first_row*W*4,c->bytes));
 if(consumer==PC_RENDER)CHECK(!memcmp(b->pixels,m->golden+(size_t)b->first_row*W*4,(size_t)b->rows*W*4));
 if(consumer==PC_RAW&&m->file)CHECK(fwrite(c->pixels,1,c->bytes,m->file)==c->bytes);
 return 1;
}
static int raw_consume(void *m,const PcChunk *c){return consume(m,c,PC_RAW);}
static int render_consume(void *m,const PcChunk *c){return consume(m,c,PC_RENDER);}
static int begin(void *context,const uint8_t *preview,size_t bytes,unsigned width,unsigned height,CwConsumer consumers[2]){
 Mock *m=context;CHECK(allowed(m));m->begin_count++;
 CHECK(width==W*2&&height==H*2&&bytes==sizeof jpg&&!memcmp(preview,jpg,bytes));
 consumers[0]=(CwConsumer){raw_consume,m};consumers[1]=(CwConsumer){render_consume,m};return m->mode!=4;
}
static int finish(void *context){Mock *m=context;CHECK(allowed(m)&&m->source_release_observed);m->finish_count++;return m->mode!=3;}
static void abort_output(void *context){Mock *m=context;CHECK(allowed(m));m->abort_count++;}
static uint16_t *full_pixels(void *context,size_t bytes){
 Mock *m=context;CHECK(allowed(m)&&bytes==(size_t)W*H*8&&!m->full_result);
 m->full_result=malloc(bytes);return m->full_result;
}
static int source_gate(void *context){return ((Mock*)context)->source_gate;}
static int notify(CsSession *s,Mock *m,unsigned index,unsigned kind){
 Image *image=m->images+(kind==0?6:index);
 X2dCaptureIdentity identity={6,index,6,0};
 X2dExposureInfo info={.exposure_id=100+index,.kind=(uint8_t)kind,.image={image,image}};
 return cs_receive(s,&info,&identity,1);
}
static int pump(CsSession *s,Mock *m){
 for(unsigned i=0;i<10000;i++){
  int state=cs_step(s,101);
  if(state==CS_PROGRESS&&cs_sources_consumed(s)&&!m->finish_count){
   for(unsigned j=0;j<7;j++)CHECK(!m->images[j].held);
   m->source_release_observed=1;
  }
  CHECK(state!=CS_REJECTED);
  if(state==CS_FAILED||state==CS_COMPLETE)return state;
  if(state==CS_WAIT)pause_poll();
 }
 CHECK(0);return CS_FAILED;
}
static void one_case(unsigned mode,const uint8_t *golden,const char *output,unsigned full){
 Mock m={.owner=thread_id(),.mode=mode,.golden=golden,.full=full};
 uint8_t mutable_preview[sizeof jpg];memcpy(mutable_preview,jpg,sizeof jpg);
 for(unsigned i=0;i<6;i++)m.images[i].view=(CmView){11836,8842,23808,1,(uint8_t*)planes[i],210510336,0};
 m.images[6].view=(CmView){3888,2918,0,8,mutable_preview,sizeof mutable_preview,0};
 CsConfig config={.task=91,.timeout_ms=1000,.width=W,.height=H,.batch=127,.working_limit=1024*1024,
  .input={retain,release,view,&m,allowed},.output={begin,finish,abort_output,&m},.compute_threads=4};
 if(full){config.full_output=config.compact_output=1;config.full_pixels=full_pixels;config.sources_released=source_gate;}
 if(mode==11){config.working_limit=1;CHECK(!cs_create(&config));return;}
 CsSession *s=cs_create(&config);CHECK(s&&!cs_destroy(s)&&cs_step(s,99)==CS_WAIT);
 CHECK(!cs_request_issued(s,92,100)&&cs_request_issued(s,91,100)&&!cs_request_issued(s,91,100));
 if(mode==12){
  X2dCaptureIdentity normal={1,0,0,0};X2dExposureInfo info={.exposure_id=999,.kind=1};
  CHECK(cs_receive(s,&info,&normal,1)==X2D_CAPTURE_NORMAL);
 }
 if(mode!=7){
  CHECK(notify(s,&m,0,0)==X2D_CAPTURE_OWN);
  CHECK(!m.images[6].held&&m.images[6].releases==1&&!m.begin_count);
  memset(mutable_preview,0,sizeof mutable_preview); /* 原厂预览早释放，后续必须使用副本。 */
 }
 if(mode==13){CHECK(notify(s,&m,0,0)==X2D_CAPTURE_CONFLICT);}
 else if(mode==6){CHECK(cs_cancel(s));}
 else for(unsigned i=0;i<(mode==8?5u:6u);i++)CHECK(notify(s,&m,i,1)==X2D_CAPTURE_OWN);
 if(mode==10){
  m.not_owner=1;CHECK(cs_step(s,101)==CS_REJECTED&&!cs_cancel(s)&&!cs_destroy(s));m.not_owner=0;
 }
 if(mode==0){m.file=fopen(output,"wb");CHECK(m.file);}
 if(mode==5){
  CHECK(cs_step(s,101)==CS_PROGRESS);
  if(full){
   while(!cs_sources_consumed(s))CHECK(cs_step(s,101)==CS_PROGRESS);
   CHECK(!m.entered[0]&&!m.entered[1]);
   CHECK(cs_step(s,101)==CS_WAIT); /* 原厂强引用尚未归还时禁止显影。 */
   m.source_release_observed=m.source_gate=1;
   CHECK(cs_step(s,101)==CS_PROGRESS);
  }
  for(unsigned i=0;(!__atomic_load_n(m.entered,__ATOMIC_ACQUIRE)||
     !__atomic_load_n(m.entered+1,__ATOMIC_ACQUIRE))&&i<10000;i++)pause_poll();
  CHECK(m.entered[0]&&m.entered[1]&&cs_cancel(s)&&!cs_destroy(s));
  CHECK(cs_step(s,101)==CS_WAIT); /* 不得在显影仍使用缓冲时提前清理。 */
  __atomic_store_n(&m.gate,1,__ATOMIC_RELEASE);
 }
 if(mode==7||mode==8)CHECK(cs_step(s,1100)==CS_FAILED);
 if(mode==9)CHECK(cs_step(s,99)==CS_FAILED);
 if(full&&mode!=5){
  for(unsigned i=0;i<100&&!cs_sources_consumed(s);i++){
   int state=cs_step(s,101);if(state==CS_FAILED)break;
  }
  if(cs_sources_consumed(s)){
   CHECK(!m.entered[0]&&!m.entered[1]&&cs_step(s,101)==CS_WAIT);
   m.source_release_observed=m.source_gate=1;
  }
 }
 int state=pump(s,&m),success=mode==0||mode==10||mode==12;
 CHECK(state==(success?CS_COMPLETE:CS_FAILED));
 if(m.file)CHECK(!fclose(m.file));
 CHECK(m.finish_count==(unsigned)(success||mode==3));
 CHECK(m.abort_count==(unsigned)(!success&&m.begin_count!=0));
 for(unsigned i=0;i<7;i++)CHECK(!m.images[i].held&&m.images[i].retains==m.images[i].releases);
 CHECK(!cs_cancel(s));CHECK(cs_step(s,9999)==state&&cs_destroy(s));
 free(m.full_result);
}
int main(int argc,char **argv){
 CHECK(argc==3);size_t bytes=(size_t)W*H*8;uint8_t *golden=malloc(bytes);CHECK(golden);
 FILE *f=fopen(argv[1],"rb");CHECK(f&&!fseek(f,4096,SEEK_SET)&&fread(golden,1,bytes,f)==bytes&&!fclose(f));
 for(unsigned i=0;i<6;i++){
  planes[i]=calloc(1,210510336);CHECK(planes[i]);
  for(unsigned y=92;y<93+H;y++)for(unsigned x=124;x<126+W;x++)
   planes[i][(size_t)y*11904+x]=(uint16_t)(x*251+y*17+i*10007);
 }
 for(unsigned full=0;full<2;full++)for(unsigned mode=0;mode<14;mode++)one_case(mode,golden,argv[2],full);
 for(unsigned i=0;i<6;i++)free(planes[i]);free(golden);
 puts("SESSION_14_CASES_FROZEN_PIXELS_OWNER_RELEASE_CANCEL_DEADLINE_AND_FAILURE_DRAIN_PASSED_OFFLINE");return 0;
}
