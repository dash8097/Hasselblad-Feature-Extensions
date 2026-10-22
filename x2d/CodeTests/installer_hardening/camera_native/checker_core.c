/* 独立 checker：实际 camera_native.c 核心；仅系统调用边界为替身。 */
#include "camera_native.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"CHECK_FAILED line=%d expr=%s\n",__LINE__,#x);exit(1);} } while(0)
static const char *job="/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-1";
struct fixture {
    int prep_rc,encode_rc,verify_rc,audio_rc,memory_ok,delay_ok,cancel;
    int cancel_prepare,cancel_memory,cancel_delay,cancel_encode,cancel_verify;
    unsigned prepare_calls,memory_calls,delay_calls,encode_calls,verify_calls,audio_calls;
    uint64_t values[50];unsigned value_count;
    char events[256],trace[256];
};
static void trace(struct fixture *f,char c){size_t n=strlen(f->trace);CHECK(n+1<sizeof(f->trace));f->trace[n]=c;f->trace[n+1]=0;}
static int prepare(void *p,const char *s){struct fixture*f=p;trace(f,'P');CHECK(!strcmp(s,job));f->prepare_calls++;if(f->cancel_prepare)f->cancel=1;return f->prep_rc;}
static int memory(void *p,uint64_t *out){struct fixture*f=p;trace(f,'M');unsigned n=f->memory_calls++;CHECK(n<50);*out=f->values[n<f->value_count?n:f->value_count-1];if(f->cancel_memory)f->cancel=1;return f->memory_ok;}
static int delay(void *p){struct fixture*f=p;trace(f,'D');f->delay_calls++;if(f->cancel_delay)f->cancel=1;return f->delay_ok;}
static int cancelled(void*p){return ((struct fixture*)p)->cancel;}
static int encode(void*p,const char*s){struct fixture*f=p;trace(f,'E');CHECK(!strcmp(s,job));f->encode_calls++;if(f->cancel_encode)f->cancel=1;return f->encode_rc;}
static int verify(void*p){struct fixture*f=p;trace(f,'V');f->verify_calls++;if(f->cancel_verify)f->cancel=1;return f->verify_rc;}
static int audio(void*p){struct fixture*f=p;trace(f,'A');f->audio_calls++;return f->audio_rc;}
static void event(void*p,const char*s){struct fixture*f=p;CHECK(strlen(s)+strlen(f->events)+2<sizeof(f->events));strcat(f->events,s);strcat(f->events,"\n");}
static struct fixture fresh(void){struct fixture f;memset(&f,0,sizeof(f));f.memory_ok=1;f.delay_ok=1;f.values[0]=573440;f.value_count=1;return f;}
static struct cn_result run(struct fixture*f,int argc,const char*const*argv){const struct cn_ops o={f,prepare,memory,delay,cancelled,encode,verify,audio,event};return cn_run(argc,argv,&o);}
static struct cn_result normal(struct fixture*f){const char*argv[]={"native","heif-disk",job};return run(f,3,argv);}
static void parsed(const char *text,int expected,uint64_t value){uint64_t actual=123;int ok=cn_mem_available(text,strlen(text),&actual);CHECK(ok==expected);if(ok)CHECK(actual==value);}

int main(void){
    CHECK(cn_job_valid(job));
    CHECK(cn_job_valid("/mnt/media_rw/ssd/multishot/native-album-ui-00000000-9999999999"));
    const char*invalid[]={"","/","/mnt/media_rw/ssd/multishot/native-album-ui-", 
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef0-1",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef012-1",
      "/mnt/media_rw/ssd/multishot/native-album-ui-ABCDEF01-1",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef0g-1",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-0",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-01",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-10000000000",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-1/",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-1/../x",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-1;id",
      "/mnt/media_rw/ssd/multishot/native-album-ui-abcdef01-1\n",
      "/mnt/media_rw/ssd/multishot2/native-album-ui-abcdef01-1"};
    CHECK(!cn_job_valid(NULL));
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)CHECK(!cn_job_valid(invalid[i]));
    /* 所有短前缀必须正常拒绝，不能越界读取。 */
    for(size_t n=0;n<strlen(job)-1;n++){char *s=malloc(n+1);CHECK(s!=NULL);memcpy(s,job,n);s[n]=0;CHECK(!cn_job_valid(s));free(s);}
    parsed("MemTotal: 1000000 kB\nMemAvailable: 573440 kB\n",1,573440);
    parsed("MemAvailable:\t0\tkB\r\n",1,0);
    parsed("MemAvailable: 18446744073709551615 kB\n",1,UINT64_MAX);
    parsed("MemAvailable: 18446744073709551616 kB\n",0,0);
    parsed("MemAvailable: 573439 kB\n",1,573439);
    parsed("MemAvailable: 573440 kB",1,573440);
    parsed("MemAvailable: 000573440 kB\n",1,573440);
    const char *bad_mem[]={"","MemFree: 600000 kB\n"," MemAvailable: 600000 kB\n",
      "MemAvailable: -1 kB\n","MemAvailable: +600000 kB\n","MemAvailable: kB\n",
      "MemAvailable: 573440\n","MemAvailable: 573440kB\n","MemAvailable: 573440 KB\n",
      "MemAvailable: 573440 k\n","MemAvailable: 573440 kB junk\n",
      "MemAvailable: 1 kB\nMemAvailable: 600000 kB\n"};
    for(unsigned i=0;i<sizeof(bad_mem)/sizeof(bad_mem[0]);i++)parsed(bad_mem[i],0,0);
    char embedded[]="MemAvailable: 600000 kB\n\0bad";uint64_t v;
    CHECK(!cn_mem_available(embedded,sizeof(embedded)-1,&v));
    CHECK(!cn_mem_available(NULL,0,&v));CHECK(!cn_mem_available("",0,NULL));
    struct fixture f=fresh();struct cn_result r;
    const char *a0[]={"native"},*a1[]={"native","wrong"},*a2[]={"native","heif-disk"},
      *a3[]={"native","heif-disk",job,"extra"},*a4[]={"native","audio","extra"},*a5[]={"native","heif-disk","bad"};
    CHECK(run(&f,1,a0).rc==2);CHECK(run(&f,2,a1).rc==2);CHECK(run(&f,2,a2).rc==2);
    CHECK(run(&f,4,a3).rc==2);CHECK(run(&f,3,a4).rc==2);CHECK(run(&f,3,a5).rc==2);CHECK(!*f.trace);
    CHECK(cn_run(0,NULL,NULL).rc==2);
    f=fresh();r=normal(&f);CHECK(r.rc==0&&r.phase==CN_DONE&&r.memory_reads==1);CHECK(!strcmp(f.trace,"PMEV"));CHECK(strstr(f.events,"HEIF_ENCODED_DIMENSIONS_VERIFIED")!=NULL);
    f=fresh();f.values[0]=573439;f.values[1]=573440;f.value_count=2;r=normal(&f);CHECK(r.rc==0&&r.memory_reads==2);CHECK(f.delay_calls==1&&f.encode_calls==1);
    f=fresh();f.values[0]=573439;r=normal(&f);CHECK(r.rc==19&&r.phase==CN_MEMORY&&r.memory_reads==50);CHECK(f.delay_calls==49&&!f.encode_calls&&!f.verify_calls);CHECK(strstr(f.events,"JPEG_RETAINED")!=NULL);
    f=fresh();f.value_count=50;for(unsigned i=0;i<49;i++)f.values[i]=573439;f.values[49]=573440;r=normal(&f);CHECK(r.rc==0&&r.memory_reads==50&&f.delay_calls==49&&f.encode_calls==1);
    f=fresh();f.values[0]=UINT64_MAX;r=normal(&f);CHECK(r.rc==0&&!f.delay_calls);
    f=fresh();f.memory_ok=0;r=normal(&f);CHECK(r.rc==19&&r.memory_reads==1&&!f.encode_calls&&!f.delay_calls);
    f=fresh();f.prep_rc=7;r=normal(&f);CHECK(r.rc==7&&r.phase==CN_VALIDATE&&!f.memory_calls&&!f.encode_calls);
    f=fresh();f.values[0]=1;f.delay_ok=0;r=normal(&f);CHECK(r.rc==1&&r.phase==CN_MEMORY&&f.delay_calls==1&&!f.encode_calls);
    const int exits[]={1,17,19,126,127,143};
    for(unsigned i=0;i<sizeof(exits)/sizeof(exits[0]);i++){f=fresh();f.encode_rc=exits[i];r=normal(&f);CHECK(r.rc==exits[i]&&r.phase==CN_ENCODE&&f.encode_calls==1&&!f.verify_calls&&!*f.events);}
    f=fresh();f.verify_rc=1;r=normal(&f);CHECK(r.rc==1&&r.phase==CN_VERIFY&&f.verify_calls==1&&!*f.events);
    /* 中断点都是系统边界注入；不模拟核心的判断或结果。 */
    for(unsigned point=0;point<6;point++){
      f=fresh();
      if(point==0)f.cancel=1;
      if(point==1)f.cancel_prepare=1;
      if(point==2)f.cancel_memory=1;
      if(point==3){f.values[0]=1;f.cancel_delay=1;f.delay_ok=0;}
      if(point==4)f.cancel_encode=1;
      if(point==5)f.cancel_verify=1;
      r=normal(&f);CHECK(r.rc==143&&r.phase!=CN_DONE);CHECK(!strstr(f.events,"DIMENSIONS_VERIFIED"));
      if(point<4)CHECK(!f.encode_calls);
    }
    const char*aa[]={"native","audio"};
    f=fresh();r=run(&f,2,aa);CHECK(r.rc==0&&f.audio_calls==1&&!f.prepare_calls&&!f.encode_calls);
    f=fresh();f.audio_rc=127;r=run(&f,2,aa);CHECK(r.rc==127&&f.audio_calls==1);
    printf("CAMERA_NATIVE_SHARED_CORE_CHECKER_PASSED checks=%u hardware=0 arm64_executed=0\n",checks);
    return 0;
}
