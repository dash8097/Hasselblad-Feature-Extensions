/* 生产交接函数，工人与原厂对象为离线替身。 */
#include "raw_stack_async.h"
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
typedef struct {void *value,*control;} X2dQtShared;
typedef struct {uint32_t exposure_id;} X2dPrivateFrame;
typedef struct {const void *data;size_t bytes;} CmView;
static RsCore core;
static unsigned dma_ok=1,dma_calls;
static uint16_t snapshot[8];
static struct {void*mapping;} dma_snapshot={snapshot};
static int rs_dma_copy(void*d,void*s,uint64_t offset,size_t bytes){(void)d;assert(s&&(uintptr_t)s<=3&&!offset&&bytes==16);dma_calls++;return dma_ok;}
static _Alignas(8) unsigned char objects[3][128];
static RsAsync accumulating;
static X2dQtShared held,accumulating_held,waiting_held;
static X2dPrivateFrame frame;
static CmView view,waiting_view;
static unsigned failed,output_done,claimed,work_ready,have_last,last_exposure,previous_exposure;
static uint64_t requested,completed,accumulating_sequence,accumulating_started,frame_received_at,waiting_sequence;
static unsigned owner_ok,metadata_ok,finish_ok,release_ok,start_ok,finish_calls,release_calls,starts,source_releases;
static unsigned copy_complete,compute_complete,compute_ok;
static char accepted[64],done[64];
static uint16_t pixels[3][8];
static int allowed(void *p){(void)p;return owner_ok;}
static void release_source(void *p,X2dQtShared *ref){(void)p;assert(owner_ok);if(!ref->control)return;memset(ref,0,sizeof *ref);source_releases++;}
static struct {int (*allowed)(void*);void (*release)(void*,X2dQtShared*);void *context;} input={allowed,release_source,NULL};
static int first_metadata(void){return metadata_ok;}
static int x2d_private_frame_finish_memory(X2dPrivateFrame *f,int ok){assert(f==&frame&&ok);finish_calls++;return finish_ok;}
static int x2d_private_frame_release(X2dPrivateFrame *f){assert(f==&frame);release_calls++;return release_ok;}
static void failure(const char *why){assert(why&&*why);failed=1;}
static void log_line(const char *line){assert(strstr(line,"RAW_STACK_"));}
static uint64_t now_ms(void){return 200;}
static int put(const char *name,const char *data,int exclusive){
 assert(!exclusive);char *target=!strcmp(name,"frame.accepted")?accepted:done;
 assert(target==accepted||!strcmp(name,"frame.done"));snprintf(target,64,"%s",data);return 1;
}
int rs_async_start(RsAsync *a,RsCore *s,const uint16_t *source,size_t bytes){
 assert(a==&accumulating&&s==&core&&source&&bytes==sizeof pixels[0]&&!a->active);
 assert(!accumulating_held.control&&!waiting_held.control&&source==snapshot);starts++;
 if(!start_ok)return 0;a->active=1;a->source=source;copy_complete=compute_complete=0;return 1;
}
int rs_async_source_read(const RsAsync *a){assert(a==&accumulating);return a->active&&copy_complete;}
int rs_async_timing(const RsAsync *a,RsAsyncTiming *t){assert(a==&accumulating&&t);*t=(RsAsyncTiming){.copy_ms=1,.total_ms=2,.copied=copy_complete,.finished=compute_complete};return 1;}
int rs_async_collect(RsAsync *a,int *ok){
 assert(a==&accumulating);if(!a->active)return -1;if(!compute_complete)return 0;
 *ok=compute_ok;a->active=0;a->source=NULL;if(compute_ok)core.frames++;return 1;
}
/* PRODUCTION_DISPATCH_FRAME */
static void incoming(unsigned n){
 void *handle=(void*)(uintptr_t)n,*map=pixels[n-1];memset(objects[n-1],0,128);memcpy(objects[n-1]+0x18,&handle,8);memcpy(objects[n-1]+0x20,&map,8);
 held=(X2dQtShared){objects[n-1],(void*)(uintptr_t)(n+1)};
 view=(CmView){pixels[n-1],sizeof pixels[0]};frame.exposure_id=n+10;
 requested=n;claimed=work_ready=1;frame_received_at=100;accepted[0]=0;
}
static void reset(void){dma_ok=1;dma_calls=0;
 memset(&core,0,sizeof core);memset(&accumulating,0,sizeof accumulating);
 memset(&held,0,sizeof held);memset(&accumulating_held,0,sizeof accumulating_held);memset(&waiting_held,0,sizeof waiting_held);
 memset(&view,0,sizeof view);memset(&waiting_view,0,sizeof waiting_view);
 requested=completed=waiting_sequence=accumulating_sequence=accumulating_started=0;
 failed=output_done=have_last=last_exposure=previous_exposure=finish_calls=release_calls=starts=source_releases=0;
 owner_ok=metadata_ok=finish_ok=release_ok=start_ok=compute_ok=1;copy_complete=compute_complete=0;accepted[0]=done[0]=0;
 incoming(1);
}
int main(void){
 reset();dispatch_frame();assert(starts==1&&accumulating.active&&!accumulating_held.control&&source_releases==1&&dma_calls==1);
 assert(!strcmp(accepted,"RS1 1\n")&&completed==1&&!claimed&&!work_ready);
 collect_frame();assert(!*done&&source_releases==1);
 incoming(2);dispatch_frame();assert(starts==1&&waiting_held.control&&completed==2&&dma_calls==1);
 incoming(3);dispatch_frame();assert(held.control&&waiting_held.control&&completed==2&&!frame_slot_available());
 compute_complete=1;collect_frame();assert(core.frames==1);dispatch_frame();assert(starts==2&&dma_calls==2&&source_releases==2&&completed==3);
 compute_complete=1;collect_frame();dispatch_frame();assert(core.frames==2&&starts==3&&source_releases==3);
 compute_complete=1;collect_frame();assert(core.frames==3&&!accumulating.active&&!failed&&!waiting_held.control&&!held.control);
 for(unsigned mode=0;mode<5;mode++){
  reset();if(mode==0)owner_ok=0;else if(mode==1)work_ready=0;else if(mode==2)claimed=0;else if(mode==3)failed=1;else output_done=1;
  dispatch_frame();assert(!starts&&!finish_calls&&held.control&&!*accepted);
 }
 reset();metadata_ok=0;dispatch_frame();assert(failed&&!finish_calls&&held.control&&!*accepted);
 reset();finish_ok=0;dispatch_frame();assert(failed&&!starts&&held.control&&!*accepted);
 reset();release_ok=0;dispatch_frame();assert(failed&&!starts&&held.control&&!*accepted);
 reset();dma_ok=0;dispatch_frame();assert(failed&&!starts&&!source_releases&&accumulating_held.control&&!*accepted);
 reset();start_ok=0;dispatch_frame();assert(failed&&source_releases==1&&!accumulating_held.control&&!*accepted);
 reset();dispatch_frame();compute_complete=1;compute_ok=0;collect_frame();assert(failed&&source_releases==1&&!*done);
 puts("RAW_STACK_DMA_SOURCE_RELEASE_BEFORE_COMPUTE_OWNER_BACKPRESSURE_AND_DRAIN_PASSED");return 0;
}
