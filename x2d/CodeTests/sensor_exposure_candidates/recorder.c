/* 第一代 X2D 4.2.0 的独立记录进程。仅用户显式启用后附着硬件执行断点。
 * 不调用相机控制，不改原厂指令，不打开设备节点，不读取用户栈或照片缓冲。
 * librcam 的当前指令仅按白名单读取两处各 4 字节。 */
#ifndef X2D_SENSOR_RECORDER_C
#define X2D_SENSOR_RECORDER_C
#include <stdint.h>
#include <stddef.h>
#include "perf_core.h"
#include "layout.h"

extern char *getenv(const char *);
extern int open(const char *,int,...),close(int),getpagesize(void);
extern long read(int,void *,unsigned long),write(int,const void *,unsigned long);
extern long pread(int,void *,unsigned long,int64_t),lseek(int,int64_t,int);
extern long readlink(const char *,char *,unsigned long),syscall(long,...);
extern void *mmap(void *,unsigned long,int,int,int,int64_t);
extern int munmap(void *,unsigned long),clock_gettime(int,void *);
extern int snprintf(char *,size_t,const char *,...);
extern int sscanf(const char *,const char *,...);
extern int ioctl(int,unsigned long,...),poll(void *,unsigned long,int);
extern void *malloc(unsigned long);
extern void free(void *);
extern int *__errno(void);
typedef void (*SigHandler)(int);
extern SigHandler signal(int,SigHandler);
extern unsigned char *SHA256(const unsigned char *,unsigned long,unsigned char *);
extern void _exit(int) __attribute__((noreturn));

#define MAX_THREADS 256u
#ifndef SENSOR_EVENTS_PER_THREAD
#define SENSOR_EVENTS_PER_THREAD 2u
#endif
#define MAX_EVENTS (MAX_THREADS*SENSOR_EVENTS_PER_THREAD)
#define MAX_SAMPLES 8192u
#define MAP_CAP 262144u
#define PERF_ENABLE 0x2400ul
#define PERF_DISABLE 0x2401ul
typedef struct {int64_t sec,nsec;} TimeSpec;
typedef struct {int fd;short events,revents;} PollFd;
typedef struct {
    int fd;unsigned site,tid;
    void *mapping;size_t length,data_size,data_offset;
    uint64_t tail,last_time,hits,ready_ns;
} Event;
static Event events[MAX_EVENTS];
static PollFd polls[MAX_EVENTS];
static unsigned event_count,total_samples,target_pid,closed_events;
static uint64_t bias,start_ticks;
static volatile int interrupted;
static const char *failure;
static int failure_errno;

static int equal(const char *a,const char *b) {
    if(!a||!b)return 0;while(*a&&*a==*b){a++;b++;}return *a==*b;
}
static int number(const char *s,unsigned *n) {
    if(!s||!*s)return 0;uint64_t v=0;
    for(;*s;s++){if(*s<'0'||*s>'9')return 0;v=v*10+(unsigned)(*s-'0');if(v>0x7fffffffu)return 0;}
    *n=(unsigned)v;return 1;
}
static uint64_t now(void) {
    TimeSpec t;if(clock_gettime(1,&t)||t.sec<0||t.nsec<0||t.nsec>=1000000000)return 0;
    return (uint64_t)t.sec*1000000000ull+(uint64_t)t.nsec;
}
static void on_signal(int sig) {(void)sig;interrupted=1;}
static int output(const char *s) {
    unsigned long n=0;while(s[n])n++;
    unsigned long done=0;
    while(done<n){long k=write(1,s+done,n-done);if(k<0&&*__errno()==4)continue;if(k<=0)return 0;done+=(unsigned long)k;}
    return 1;
}
static int fail(const char *s) {if(!failure){failure=s;failure_errno=*__errno();}return 0;}
static long file_read(const char *path,char *buffer,size_t capacity) {
    int fd=open(path,0x80000);if(fd<0)return -1;size_t count=0;long rc=0;
    while(count<capacity-1){rc=read(fd,buffer+count,capacity-1-count);if(rc<0&&*__errno()==4)continue;if(rc<0){close(fd);return -1;}if(!rc)break;count+=(size_t)rc;}
    char extra;if(count==capacity-1 && read(fd,&extra,1)!=0){close(fd);return -1;}
    close(fd);buffer[count]=0;return (long)count;
}
static int is_true_process(void) {
    char cmd[64];long n=file_read("/proc/self/cmdline",cmd,sizeof cmd);
    return n==17&&equal(cmd,"/system/bin/true");
}
static int identity(uint64_t *ticks) {
    char path[80],exe[128],stat[2048];
    snprintf(path,sizeof path,"/proc/%u/exe",target_pid);
    long n=readlink(path,exe,sizeof exe-1);if(n<0||n>=(long)sizeof exe-1)return 0;
    exe[n]=0;if(!equal(exe,"/system/bin/camera-service"))return 0;
    snprintf(path,sizeof path,"/proc/%u/stat",target_pid);
    n=file_read(path,stat,sizeof stat);if(n<=0)return 0;
    char *last=0;for(char *p=stat;*p;p++)if(*p==')')last=p;
    if(!last)return 0;char *p=last+1;
    for(unsigned field=3;field<=22;field++){
        while(*p==' ')p++;if(!*p)return 0;char *q=p;while(*q&&*q!=' '&&*q!='\n')q++;
        if(field==22){uint64_t v=0;if(p==q)return 0;for(;p<q;p++){if(*p<'0'||*p>'9'||v>UINT64_MAX/10)return 0;unsigned d=(unsigned)(*p-'0');if(v==UINT64_MAX/10&&d>UINT64_MAX%10)return 0;v=v*10+d;}*ticks=v;return v!=0;}
        p=q;
    }return 0;
}
static int threads(unsigned list[MAX_THREADS],unsigned *count) {
    char path[80];snprintf(path,sizeof path,"/proc/%u/task",target_pid);
    /* ARM64 O_DIRECTORY=040000；0x10000 在此 ABI 是 O_DIRECT。 */
    int fd=open(path,0x84000);if(fd<0)return 0;unsigned used=0;unsigned char data[4096];
    for(;;){long n=syscall(61,fd,data,(unsigned long)sizeof data);if(n<0&&*__errno()==4)continue;if(n<0){close(fd);return 0;}if(!n)break;
        for(long pos=0;pos<n;){
            if(n-pos<20){close(fd);return 0;}unsigned len=(unsigned)sensor_le(data+pos+16,2);
            if(len<20||len>(unsigned)(n-pos)){close(fd);return 0;}
            char *name=(char *)data+pos+19;unsigned max=len-19,j=0;while(j<max&&name[j])j++;
            if(j==max){close(fd);return 0;}unsigned tid;
            if(number(name,&tid)&&tid){if(used==MAX_THREADS){close(fd);return 0;}list[used++]=tid;}
            pos+=len;
        }
    }
    close(fd);if(!used)return 0;
    for(unsigned i=1;i<used;i++){unsigned v=list[i],j=i;while(j&&list[j-1]>v){list[j]=list[j-1];j--;}list[j]=v;}
    for(unsigned i=1;i<used;i++)if(list[i]==list[i-1])return 0;
    *count=used;return 1;
}
static int same_threads(const unsigned *original,unsigned count) {
    unsigned list[MAX_THREADS],n=0;if(!threads(list,&n)||n!=count)return 0;
    for(unsigned i=0;i<n;i++)if(list[i]!=original[i])return 0;return 1;
}
static int source_and_mapping_sites(const unsigned *sites,unsigned site_count) {
    if(!sites||!site_count||site_count>SENSOR_EVENTS_PER_THREAD)return fail("site_count");
    for(unsigned i=0;i<site_count;i++)if(sites[i]>=sizeof sensor_sites/sizeof sensor_sites[0])return fail("site_index");
    static const unsigned char expected[32]=RCAM_SHA_BYTES;
    unsigned char digest[32];int fd=open("/system/lib64/librcam.so",0x80000);
    if(fd<0)return fail("source_open");long length=lseek(fd,0,2);
    if(length!=RCAM_FILE_SIZE){close(fd);return fail("source_size");}
    void *data=mmap(0,(unsigned long)length,1,2,fd,0);close(fd);
    if(data==(void *)-1)return fail("source_map");
    int valid=SHA256(data,(unsigned long)length,digest)!=0;
    for(unsigned i=0;i<32;i++)if(digest[i]!=expected[i])valid=0;
    munmap(data,(unsigned long)length);if(!valid)return fail("source_hash");
    char path[80];snprintf(path,sizeof path,"/proc/%u/maps",target_pid);
    char *maps=malloc(MAP_CAP);if(!maps)return fail("maps_allocate");
    long n=file_read(path,maps,MAP_CAP);if(n<0){free(maps);return fail("maps_read");}
    unsigned matches=0;uint64_t mapping_end=0;
    for(char *line=maps;*line;){char *end=line;while(*end&&*end!='\n')end++;char saved=*end;*end=0;
        unsigned long long a=0,z=0,off=0;char permissions[5]={0},mapped_path[160]={0};
        int fields=sscanf(line,"%llx-%llx %4s %llx %*s %*s %159[^\n]",&a,&z,permissions,&off,mapped_path);
        if(fields==5&&equal(permissions,"r-xp")&&equal(mapped_path,"/system/lib64/librcam.so")&&off==RCAM_EXEC_PAGE_OFFSET){
            if(a<RCAM_EXEC_PAGE_VA||a>=z||a>=(1ull<<48)){free(maps);return fail("mapping_bounds");}
            bias=a-RCAM_EXEC_PAGE_VA;mapping_end=z;matches++;
        }
        if(!saved)break;line=end+1;
    }
    free(maps);if(matches!=1)return fail("mapping_ambiguous");
    snprintf(path,sizeof path,"/proc/%u/mem",target_pid);fd=open(path,0x80000);
    if(fd<0)return fail("instruction_read_open");
    for(unsigned i=0;i<site_count;i++){
        const SensorSite *site=&sensor_sites[sites[i]];uint32_t word=0;
        uint64_t checked_bias=0,address=0;
        if(!sensor_load_bias(bias+RCAM_EXEC_PAGE_VA,mapping_end,RCAM_EXEC_PAGE_OFFSET,
                RCAM_EXEC_PAGE_VA,RCAM_EXEC_PAGE_OFFSET,site->elf_va,&checked_bias,&address)||checked_bias!=bias||
                pread(fd,&word,4,(int64_t)address)!=4||word!=site->word){close(fd);return fail("live_instruction_mismatch");}
    }
    close(fd);return 1;
}
static int source_and_mapping(const SensorGroup *group) {
    return source_and_mapping_sites(group->site,2);
}
static int attach(unsigned tid,unsigned site,unsigned page) {
    if(event_count==MAX_EVENTS)return fail("event_limit");
    Event *e=&events[event_count];*e=(Event){.fd=-1,.site=site,.tid=tid};
    SensorPerfAttr a=sensor_attr(bias+sensor_sites[site].elf_va);
    long fd=syscall(241,&a,(int)tid,-1,-1,8ul);
    if(fd<0)return fail("perf_event_open");e->fd=(int)fd;event_count++;
    e->length=(size_t)page*3;e->mapping=mmap(0,e->length,3,1,e->fd,0);
    if(e->mapping==(void *)-1){e->mapping=0;return fail("perf_ring_map");}
    unsigned char *m=e->mapping;
    e->data_offset=(size_t)sensor_le(m+1040,8);e->data_size=(size_t)sensor_le(m+1048,8);
    if(e->data_offset<page||e->data_offset>e->length||e->data_size>e->length-e->data_offset||
       e->data_size<128||(e->data_size&(e->data_size-1)))return fail("perf_ring_layout");
    polls[event_count-1]=(PollFd){e->fd,1,0};return 1;
}
static int known_rva(uint64_t address,uint64_t *rva) {
    if(address<bias||address-bias<RCAM_EXEC_PAGE_VA||address-bias>=RCAM_EXEC_END)return 0;*rva=address-bias;return 1;
}
static int publish(const Event *e,const SensorRecord *r) {
    char line[640],fields[240]={0};unsigned site=e->site;const char *id=sensor_sites[site].name;
    uint64_t rva=0,callback=0;int caller=known_rva(r->regs[5],&rva);
    int w0=(int)(uint32_t)r->regs[0];unsigned w1=(unsigned)r->regs[1],w19=(unsigned)r->regs[3],w20=(unsigned)r->regs[4];
    if(site==SITE_tg_entry)snprintf(fields,sizeof fields,",\"requested\":%u",(unsigned)w0);
    else if(site==SITE_tg_written)snprintf(fields,sizeof fields,",\"written\":%u",w19);
    else if(site==SITE_channel_entry)snprintf(fields,sizeof fields,",\"channel\":%u,\"requested\":%u",(unsigned)w0,w1);
    else if(site==SITE_channel_written)snprintf(fields,sizeof fields,",\"channel\":%u,\"written_mask\":%u",w20,w19);
    else if(site==SITE_vsync_entry)snprintf(fields,sizeof fields,",\"channel\":%u",(unsigned)w0);
    else if(site==SITE_bridge_entry||site==SITE_longexp_entry)snprintf(fields,sizeof fields,",\"requested\":%u",w1);
    else if(site==SITE_exposure_committed||site==SITE_bridge_return||site==SITE_longexp_return||site==SITE_irq_received)
        snprintf(fields,sizeof fields,",\"result\":%d",w0);
    else if(site==SITE_sof||site==SITE_eof||site==SITE_irq_dispatch)snprintf(fields,sizeof fields,",\"irq_code\":%u",(unsigned)w0);
    char caller_text[32]="null",callback_text[32]="null";
    /* caller_elf_va 仅入口/完整返回有定义；写后寄存器的 LR 不作为调用者输出。 */
    if(caller&&sensor_sites[site].entry)snprintf(caller_text,sizeof caller_text,"%llu",(unsigned long long)rva);
    if(site==SITE_irq_dispatch&&known_rva(r->regs[2],&callback))snprintf(callback_text,sizeof callback_text,"%llu",(unsigned long long)callback);
    uint64_t received=now();if(!received||received<r->time_ns)return fail("clock_domain");
    int n=snprintf(line,sizeof line,
        "{\"kind\":\"sample\",\"site\":\"%s\",\"elf_va\":%llu,\"pid\":%u,\"tid\":%u,\"cpu\":%u,\"time_ns\":%llu,\"receipt_ns\":%llu,\"caller_elf_va\":%s,\"callback_elf_va\":%s%s}\n",
        id,(unsigned long long)sensor_sites[site].elf_va,r->pid,r->tid,r->cpu,(unsigned long long)r->time_ns,
        (unsigned long long)received,caller_text,callback_text,fields);
    return n>0&&n<(int)sizeof line&&output(line)?1:fail("output_write");
}
static int drain(Event *e) {
    unsigned char record[4096];unsigned char *m=e->mapping;
    uint64_t head=__atomic_load_n((uint64_t *)(m+1024),__ATOMIC_ACQUIRE);
    while(e->tail!=head){size_t length=0;
        int copied=sensor_ring_copy(m+e->data_offset,e->data_size,head,e->tail,record,&length);
        if(copied!=1)return fail("ring_incomplete_or_overrun");SensorRecord r;
        int parsed=sensor_record(record,length,&r);
        if(parsed==2)return fail("lost_samples");
        if(parsed==3){
            if(r.pid==target_pid)return fail("task_lifecycle_changed");
            e->tail+=r.size;__atomic_store_n((uint64_t *)(m+1032),e->tail,__ATOMIC_RELEASE);continue;
        }
        if(parsed!=1)return fail("record_abi_or_type");
        if(r.pid!=target_pid||r.tid!=e->tid||r.ip!=bias+sensor_sites[e->site].elf_va||
           (e->last_time&&r.time_ns<e->last_time))return fail("sample_identity_or_order");
        e->last_time=r.time_ns;e->hits++;if(++total_samples>MAX_SAMPLES)return fail("sample_limit");
        if(!publish(e,&r))return 0;e->tail+=r.size;
        __atomic_store_n((uint64_t *)(m+1032),e->tail,__ATOMIC_RELEASE);
    }return 1;
}
static void cleanup_mode(int verify_remaining) {
    for(unsigned i=0;i<event_count;i++){
        Event *e=&events[i];if(e->fd>=0){
            if(ioctl(e->fd,PERF_DISABLE,0)<0)fail("disable_failed");
        }
    }
    for(unsigned i=0;i<event_count;i++){
        Event *e=&events[i];if(verify_remaining&&e->mapping&&!failure)drain(e);
        if(verify_remaining&&e->fd>=0&&!failure){uint64_t counter=0;long n=read(e->fd,&counter,8);
            if(n!=8||counter!=e->hits)fail("counter_sample_mismatch");}
        if(e->mapping){if(munmap(e->mapping,e->length)<0)fail("unmap_failed");e->mapping=0;}
        if(e->fd>=0){if(close(e->fd)<0)fail("close_failed");else closed_events++;e->fd=-1;}
    }
}
static void cleanup(void) {cleanup_mode(1);}
static int run(void) {
    const char *name=getenv("X2D_SENSOR_GROUP");const SensorGroup *group=0;
    for(unsigned i=0;i<SENSOR_GROUP_COUNT;i++)if(equal(name,sensor_groups[i].name))group=&sensor_groups[i];
    unsigned seconds=0;
    if(!group||!number(getenv("X2D_SENSOR_PID"),&target_pid)||!target_pid||
       !number(getenv("X2D_SENSOR_SECONDS"),&seconds)||!seconds||seconds>60)return fail("arguments");
    uint64_t first=now();if(!first||!identity(&start_ticks))return fail("target_identity");
    if(!source_and_mapping(group))return 0;
    unsigned tids[MAX_THREADS],count=0;
    if(!threads(tids,&count))return fail("thread_snapshot");int page=getpagesize();
    if(page<4096||page>65536||(page&(page-1)))return fail("page_size");
    for(unsigned i=0;i<count;i++)for(unsigned j=0;j<2;j++)if(!attach(tids[i],group->site[j],(unsigned)page))return 0;
    uint64_t ticks=0;if(!identity(&ticks)||ticks!=start_ticks||!same_threads(tids,count))return fail("target_changed_before_arm");
    for(unsigned i=0;i<event_count;i++)if(ioctl(events[i].fd,PERF_ENABLE,0)<0)return fail("enable_failed");
    uint64_t ready=now();if(!ready||ready>UINT64_MAX-(uint64_t)seconds*1000000000ull)return fail("clock_start");
    char header[640];int n=snprintf(header,sizeof header,
        "{\"kind\":\"header\",\"schema\":\"X2D_SENSOR_SAMPLE_R1\",\"group\":\"%s\",\"firmware\":\"4.2.0\",\"librcam_sha256\":\"" RCAM_SHA_TEXT "\",\"pid\":%u,\"threads\":%u,\"seconds\":%u,\"ready_ns\":%llu,\"clock\":\"CLOCK_MONOTONIC\",\"hardware_breakpoints_per_thread\":2,\"flash_enabled\":false}\n",
        name,target_pid,count,seconds,(unsigned long long)ready);
    if(n<=0||n>=(int)sizeof header||!output(header))return fail("header_write");
    signal(2,on_signal);signal(15,on_signal);
    uint64_t deadline=ready+(uint64_t)seconds*1000000000ull,next_check=ready,last=ready;
    while(!interrupted){
        uint64_t t=now();if(!t||t<last)return fail("clock_backward");last=t;
        if(t>=deadline)break;
        if(t>=next_check){
            if(!identity(&ticks)||ticks!=start_ticks||!same_threads(tids,count))return fail("target_or_threads_changed");
            next_check=t+250000000ull;
        }
        int p=poll(polls,event_count,10);if(p<0&&*__errno()!=4)return fail("poll_failed");
        for(unsigned i=0;i<event_count;i++){
            if(polls[i].revents&(8|16|32))return fail("event_unavailable");
            if(!drain(&events[i]))return 0;
        }
    }
    if(interrupted)return fail("user_interrupted");
    if(!identity(&ticks)||ticks!=start_ticks||!same_threads(tids,count))return fail("target_changed_at_end");
    return 1;
}
#ifndef X2D_SENSOR_TEST
__attribute__((constructor)) static void sensor_main(void) {
    if(!equal(getenv("X2D_SENSOR_RECORD"),"4.2.0")||!is_true_process())return;
    int success=run();uint64_t end=now();cleanup();
    char footer[320];snprintf(footer,sizeof footer,
        "{\"kind\":\"footer\",\"complete\":%s,\"samples\":%u,\"end_ns\":%llu,\"reason\":\"%s\",\"errno\":%d,\"closed_events\":%u}\n",
        success&&!failure?"true":"false",total_samples,(unsigned long long)end,failure?failure:"duration_complete",failure_errno,closed_events);
    int wrote=output(footer);_exit(success&&!failure&&wrote?0:20);
}
#endif
#endif /* X2D_SENSOR_RECORDER_C */
