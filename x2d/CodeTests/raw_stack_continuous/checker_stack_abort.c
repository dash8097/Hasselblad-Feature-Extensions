/* 实现者 abort 控制逻辑模型；不是原厂 Qt/HAL 运行证明。 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
enum {X2D_FRAME_EMPTY=0};
typedef struct {void *value,*control;} Ref;
typedef struct {int status,memory_finished,submitted;} Frame;
static struct {int (*allowed)(void*);void(*release)(void*,Ref*);void*context;} input;
static struct {int active;} accumulating;
static Ref accumulating_held,waiting_held,held;
static struct {int ignored;} waiting_view,view,metadata;
static Frame frame;
static struct {int uncertain;} dma_snapshot;
static int abort_done,aborting,claimed,work_ready,output_done;
static uint64_t requested,completed;
static unsigned char *preview;static size_t preview_bytes;
static int owner_ok,async_done,pool_ok,finish_ok,frame_ok,put_ok,release_calls,pool_calls,marker_calls;
static const char *request;
static int path(char*out,size_t cap,const char*n){return snprintf(out,cap,"/tmp/%s",n)>0;}
static int text_read(const char*n,char*out,size_t cap){(void)n;if(!request)return -1;size_t len=strlen(request);if(len>=cap)return -1;memcpy(out,request,len+1);return (int)len;}
static void failure(const char*s){(void)s;}static void log_line(const char*s){(void)s;}
static int allowed(void*x){(void)x;return owner_ok;}
static void release(void*x,Ref*r){(void)x;release_calls++;r->value=r->control=NULL;}
static int rs_async_collect(void*a,int*ok){(void)a;*ok=1;if(!async_done)return 0;accumulating.active=0;return 1;}
static int x2d_private_frame_finish_memory(Frame*f,int success){(void)success;if(finish_ok)f->memory_finished=1;return finish_ok;}
static int x2d_private_frame_release(Frame*f){if(f->status&&(!frame_ok||(!f->memory_finished&&!f->submitted)))return 0;memset(f,0,sizeof *f);return 1;}
static int pool_release(void){pool_calls++;return pool_ok;}
static void ch_metadata_destroy(void*m){(void)m;}
static int put(const char*n,const char*v,int exclusive){(void)n;(void)v;(void)exclusive;marker_calls++;return put_ok;}
#include "native_stack_abort.inc"
static void reset(void){dma_snapshot.uncertain=0;memset(&accumulating,0,sizeof accumulating);memset(&frame,0,sizeof frame);memset(&held,0,sizeof held);memset(&waiting_held,0,sizeof waiting_held);memset(&accumulating_held,0,sizeof accumulating_held);abort_done=aborting=claimed=work_ready=output_done=release_calls=pool_calls=marker_calls=0;requested=1;completed=0;preview=NULL;preview_bytes=0;owner_ok=async_done=pool_ok=finish_ok=frame_ok=put_ok=1;request="RAW_STACK_ABORT_IDLE_V1\n";input.allowed=allowed;input.release=release;input.context=NULL;}
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("STACK_ABORT_MODEL_FAIL line=%d\n",__LINE__);return 1;}}while(0)
int checker_stack_abort(void){
 reset();CHECK(stack_abort_poll()==1&&abort_done&&marker_calls==1&&requested==completed);
 reset();claimed=0;work_ready=0;CHECK(stack_abort_poll()==1&&abort_done&&pool_calls==1);
 reset();accumulating.active=1;accumulating_held.control=(void*)1;async_done=0;CHECK(stack_abort_poll()==1&&!abort_done&&!release_calls&&!pool_calls);async_done=1;CHECK(stack_abort_poll()==1&&abort_done&&release_calls==1);
 reset();waiting_held.control=(void*)1;held.control=(void*)1;frame.status=1;frame.submitted=1;CHECK(stack_abort_poll()==1&&abort_done&&release_calls==2&&!frame.status);
 reset();frame.status=1;frame.submitted=0;held.control=(void*)1;CHECK(stack_abort_poll()==1&&!abort_done&&!release_calls&&!pool_calls&&held.control);
 reset();frame.status=1;frame.memory_finished=1;CHECK(stack_abort_poll()==1&&abort_done);
 reset();owner_ok=0;held.control=(void*)1;CHECK(stack_abort_poll()==1&&!abort_done&&!release_calls&&!pool_calls);
 reset();pool_ok=0;CHECK(stack_abort_poll()==1&&!abort_done&&!marker_calls);
 reset();put_ok=0;CHECK(stack_abort_poll()==1&&!abort_done&&marker_calls==1);
 reset();request="BAD\n";CHECK(stack_abort_poll()==1&&!aborting&&!abort_done&&!pool_calls);
 reset();CHECK(stack_abort_poll()==1&&abort_done);CHECK(stack_abort_poll()==1&&marker_calls==1&&pool_calls==1);
 reset();frame.status=1;frame.submitted=1;finish_ok=0;CHECK(stack_abort_poll()==1&&!abort_done&&!pool_calls);finish_ok=1;frame_ok=0;CHECK(stack_abort_poll()==1&&!abort_done&&!pool_calls);
 reset();dma_snapshot.uncertain=1;accumulating_held.control=(void*)1;CHECK(stack_abort_poll()==1&&!abort_done&&!release_calls&&!pool_calls&&accumulating_held.control);
 printf("STACK_ABORT_MODEL_PASSED cases=13 checks=%u\n",checks);return 0;
}
