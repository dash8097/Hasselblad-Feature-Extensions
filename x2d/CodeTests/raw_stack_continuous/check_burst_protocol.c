/* 执行实际接收端的 epoch/压力/归还协议；文件及 RAW 引用为离线替身。 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#undef NDEBUG
#include <assert.h>
typedef struct {void *control;} Ref;
static Ref held,waiting_held,accumulating_held;
static struct {int active;} accumulating;
static unsigned failed,aborting,output_done,claimed,iso_target;
static uint64_t requested,completed,burst_epoch,burst_closed;
static unsigned burst_peak_refs,burst_pressure_sent,burst_drain_sent;
static char request[96],closed_text[96],armed[96],drained[96],pressure[96];
static int pool_ok=1,write_ok=1,read_bad;
static unsigned allocations,writes;
static void failure(const char *reason){assert(reason&&*reason);failed=1;}
static int pool_allocate(void){allocations++;return pool_ok;}
static int path(char *out,size_t cap,const char *name){return snprintf(out,cap,"%s",name)>0;}
static int fake_lstat(const char *name,struct stat *out){(void)out;const char *p=!strcmp(name,"burst.request")?request:closed_text;if(*p)return 0;errno=ENOENT;return -1;}
#define lstat fake_lstat
static int text_read(const char *name,char *out,size_t cap){
 const char *p=!strcmp(name,"burst.request")?request:closed_text;
 if(!*p||read_bad)return -1;if(strlen(p)>=cap)return -1;strcpy(out,p);return (int)strlen(p);
}
static int put(const char *name,const char *text,int exclusive){
 assert(!exclusive);writes++;if(!write_ok)return 0;
 char *p=!strcmp(name,"burst.armed")?armed:!strcmp(name,"burst.drained")?drained:pressure;
 assert(p!=pressure||!strcmp(name,"burst.pressure"));strcpy(p,text);return 1;
}
/* PRODUCTION_PARSE_UINT */
/* PRODUCTION_BURST_PROTOCOL */
static void reset(void){
 held.control=waiting_held.control=accumulating_held.control=NULL;accumulating.active=0;
 failed=aborting=output_done=claimed=iso_target=0;requested=completed=burst_epoch=burst_closed=0;
 burst_peak_refs=burst_pressure_sent=burst_drain_sent=0;
 request[0]=closed_text[0]=armed[0]=drained[0]=pressure[0]=0;
 pool_ok=write_ok=1;read_bad=0;allocations=writes=0;
}
static void arm(void){strcpy(request,"RSB1 1\n");burst_poll();assert(!failed&&burst_epoch==1&&!strcmp(armed,request));}
int main(void){
 reset();burst_poll();assert(!failed&&!allocations&&!writes);
 for(unsigned reason=0;reason<9;reason++){
  reset();strcpy(request,"RSB1 1\n");
  if(reason==0)claimed=1;if(reason==1)held.control=(void*)1;if(reason==2)waiting_held.control=(void*)1;
  if(reason==3)accumulating_held.control=(void*)1;if(reason==4)accumulating.active=1;
  if(reason==5)requested=1;if(reason==6)iso_target=100;if(reason==7)pool_ok=0;if(reason==8)read_bad=1;
  burst_poll();assert(failed&&!burst_epoch&&!armed[0]);
 }
 const char *invalid[]={"RSB1 0\n","RSB1 01\n","RSB1 2\n","RSB1 -1\n","RSB1 1","RSB1 1\n\n","RSB1 18446744073709551616\n","garbage"};
 for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){reset();strcpy(request,invalid[i]);burst_poll();assert(failed&&!burst_epoch);}
 reset();arm();unsigned before=writes;burst_poll();assert(writes==before&&allocations==1);
 held.control=(void*)1;burst_pressure();assert(burst_peak_refs==1&&!strcmp(pressure,"RSB1 1\n"));
 waiting_held.control=(void*)2;burst_pressure();assert(!strcmp(pressure,"RSB1 1\n")&&burst_peak_refs==2);
 before=writes;accumulating_held.control=(void*)3;burst_pressure();assert(writes==before&&burst_peak_refs==3&&!failed);
 strcpy(closed_text,"RSB1 1\n");burst_poll();assert(!drained[0]);
 held.control=waiting_held.control=accumulating_held.control=NULL;accumulating.active=1;burst_poll();assert(!drained[0]);
 accumulating.active=0;requested=2;completed=1;burst_poll();assert(!drained[0]);
 completed=2;burst_poll();assert(!failed&&!strcmp(drained,"RSB1 1\n"));before=writes;burst_poll();assert(writes==before);
 strcpy(request,"RSB1 2\n");burst_poll();assert(!failed&&burst_epoch==2&&!strcmp(armed,request)&&!burst_pressure_sent&&!burst_drain_sent);
 strcpy(request,"RSB1 3\n");burst_poll();assert(failed&&burst_epoch==2);
 for(unsigned i=0;i<3;i++){reset();arm();strcpy(closed_text,i==0?"RSB1 2\n":i==1?"RSB1 0\n":"RSB1 1\n");read_bad=i==2;burst_poll();assert(failed);}
 reset();write_ok=0;strcpy(request,"RSB1 1\n");burst_poll();assert(failed&&!armed[0]);
 reset();failed=1;strcpy(request,"RSB1 1\n");burst_poll();assert(!allocations&&!writes);
 /* RAW 已复制归还，但累计线程仍占用工作槽；不能把它当空闲。 */
 reset();arm();accumulating.active=1;waiting_held.control=(void*)2;
 burst_pressure();assert(!failed&&burst_peak_refs==1&&!strcmp(pressure,"RSB1 1\n"));
 /* 下一曝光的预览先到、RAW 未到，也已经占用 claimed 槽。 */
 reset();arm();accumulating.active=1;claimed=1;
 burst_pressure();assert(!failed&&!strcmp(pressure,"RSB1 1\n"));
 puts("BURST_PROTOCOL_PASS actual_epoch_pressure_drain_functions malformed_rearm_backpressure_and_io_failures");return 0;
}
