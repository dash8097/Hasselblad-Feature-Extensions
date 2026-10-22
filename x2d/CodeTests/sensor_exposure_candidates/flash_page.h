/* 独立测试页的会话与状态传输；前按钮处理复用历史实现的固定副本。 */
#ifndef X2D_SENSOR_FLASH_PAGE_H
#define X2D_SENSOR_FLASH_PAGE_H
#include "flash_core.h"
#include "flash_front_core.h"
extern int rename(const char *,const char *),unlink(const char *);
#define PAGE_DIR "/data/local/tmp/x2d-sensor-flash-r2/"
static unsigned page_pid,front_enabled;
static uint64_t page_start;
static const char *page_session;
static int page_mode,bridge_failed;
static const char *sampling_error;
static int sampling_errno;
static unsigned sampling_retryable,thread_refreshes,sampled_threads,requested_enabled;
static X2DFrontCore front;
static uint64_t now_ms(void){return now()/1000000ull;}
static int same(const unsigned char*a,const unsigned char*b,size_t n){for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static int x2d_test_uint(const char **p,unsigned *v){return flash_uint(p,v);}
static int x2d_test_literal(const char **p,const char *v){return flash_literal(p,v);}
static int read_text(const char *p,char *b,unsigned n){long got=file_read(p,b,n);return got>0?(int)got:0;}
static int uint64_text(const char *p,uint64_t *out){
    uint64_t n=0;if(!p||!*p)return 0;
    for(;*p;p++){if(*p<'0'||*p>'9'||n>(UINT64_MAX-(unsigned)(*p-'0'))/10)return 0;n=n*10+(unsigned)(*p-'0');}
    *out=n;return n!=0;
}
static int page_identity(void){
    char path[80],b[2048];snprintf(path,sizeof path,"/proc/%u/stat",page_pid);
    if(read_text(path,b,sizeof b)<=0)return 0;
    char *p=0;for(char *q=b;*q;q++)if(*q==')')p=q;
    if(!p||p[1]!=' ')return 0;p+=2;
    for(unsigned i=0;i<19;i++){while(*p&&*p!=' ')p++;if(!*p)return 0;p++;}
    char *end=p;while(*end&&*end!=' ')end++;*end=0;
    uint64_t start;if(!uint64_text(p,&start)||start!=page_start)return 0;
    snprintf(path,sizeof path,"/proc/%u/cmdline",page_pid);
    int n=read_text(path,b,sizeof b);if(n<=0||!equal(b,"/system/bin/camera-gui"))return 0;
    unsigned image=0,none=0;
    for(int i=0;i<n;){int j=i;while(j<n&&b[j])j++;if(j==n)return 0;
        if(equal(b+i,"--imagetest"))image=1;
        if(equal(b+i,"--bus")&&j+1<n&&equal(b+j+1,"none"))none=1;i=j+1;}
    return image&&none;
}
static int page_initialize(void){
    const char *pid=getenv("X2D_FLASH_PAGE_PID"),*start=getenv("X2D_FLASH_PAGE_START");
    page_session=getenv("X2D_FLASH_SESSION");
    if(!number(pid,&page_pid)||!page_pid||!uint64_text(start,&page_start)||!page_session)return -1;
    unsigned length=0;for(const char *p=page_session;*p;p++){if((*p<'0'||*p>'9')&&*p!='-')return -1;if(++length>63)return -1;}
    if(!length||!page_identity())return -1;
    int old=open(PAGE_DIR "status.json",0x20000);if(old>=0){close(old);return -1;}
    int fd=open(PAGE_DIR "page.log",0x20800);if(fd<0)return -1;
    if(lseek(fd,0,2)<0){close(fd);return -1;}page_mode=1;return fd;
}
static int file_hash(const char *path,const char *hex,long minimum,long maximum){
    int fd=open(path,0x20000);if(fd<0)return 0;long n=lseek(fd,0,2);
    if(n<minimum||n>maximum){close(fd);return 0;}
    void *data=mmap(0,(unsigned long)n,1,2,fd,0);close(fd);if(data==(void*)-1)return 0;
    unsigned char h[32];int ok=SHA256(data,(unsigned long)n,h)!=0;munmap(data,(unsigned long)n);
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<32&&ok;i++)if(digits[h[i]>>4]!=hex[i*2]||digits[h[i]&15]!=hex[i*2+1])ok=0;
    return ok;
}
static void page_status(const FlashCore *g,const char *reason,unsigned mask,unsigned sampling,unsigned helper_ok,uint64_t actual_ns){
    if(!page_mode||bridge_failed)return;
    char b[1600];int n=snprintf(b,sizeof b,
      "{\"session\":\"%s\",\"monotonic_ms\":%llu,\"sequence\":%u,\"enabled\":%u,\"requested_enabled\":%u,\"source\":%u,\"delay_us\":%u,\"pending\":%u,\"cycle\":%u,\"helper_attempts\":%u,\"matched\":%u,\"skipped\":%u,\"allowed_mask\":%u,\"sampling\":%u,\"last_helper_ok\":%u,\"event_ns\":%llu,\"target_ns\":%llu,\"actual_ns\":%llu,\"late_us\":%llu,\"front_enabled\":%u,\"front_ready\":%u,\"front_armed\":%u,\"visible_seq\":%u,\"requested_visible\":%u,\"reason\":\"%s\",\"sampling_error\":\"%s\",\"sampling_errno\":%d,\"sampling_retryable\":%u,\"sampled_threads\":%u,\"thread_refreshes\":%u}\n",
      page_session,(unsigned long long)now_ms(),g->sequence,g->enabled,requested_enabled,g->source,g->delay_us,g->pending,g->cycle,
      g->attempts,g->matched,g->skipped,mask,sampling,helper_ok,(unsigned long long)g->event_ns,
      (unsigned long long)g->deadline_ns,(unsigned long long)actual_ns,(unsigned long long)(g->late_ns/1000),
      front_enabled,front.ready,front.armed,front.request_seq,front.desired,reason,sampling_error?sampling_error:"",sampling_errno,sampling_retryable,sampled_threads,thread_refreshes);
    if(n<=0||n>=(int)sizeof b){bridge_failed=1;return;}
    int fd=open(PAGE_DIR "status.next",0x200c1,0600);if(fd<0){bridge_failed=1;return;}
    long wrote=write(fd,b,(unsigned)n);close(fd);
    if(wrote!=n||rename(PAGE_DIR "status.next",PAGE_DIR "status.json"))bridge_failed=1;
}
#endif
