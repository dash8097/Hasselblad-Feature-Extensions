/* 仅独立 /system/bin/true 子进程，用户手工启动并在页面开启后试闪。 */
#define X2D_SENSOR_TEST 1
#define SENSOR_EVENTS_PER_THREAD 3u
#include "recorder.c"
#include "flash_page.h"
#include "exposure_front_input.c"
extern void *dlopen(const char*,int),*dlsym(void*,const char*);
extern int dlclose(void*);
extern int ppoll(void*,unsigned long,const void*,const void*);
extern void qsort(void*,size_t,size_t,int(*)(const void*,const void*));
typedef struct {uint64_t before_high_us,after_high_us,after_low_us;} PulseTiming;
static void *pulse_library;
static int (*pulse_init)(void),(*pulse_close)(void),(*pulse_fire)(PulseTiming*);
static FlashCore core;
static unsigned allowed_mask,sampling,helper_ok,tids[MAX_THREADS],tid_count;
static uint64_t actual_ns,last_ping_ns,next_refresh_ns,retry_sampling_ns;
static int mem_fd=-1;
typedef struct {FlashEvent event;uint64_t coverage_ns;} QueuedFlash;
static QueuedFlash queue[MAX_SAMPLES];static unsigned queued;
static unsigned active_sites[3],active_site_count,sampling_page,thread_dirty,changed_tids[MAX_THREADS],changed_count;
#define HELPER "/system/lib64/libx2d_sensor_hotshoe_r2.so"
static int resident_init(void){
    if(!file_hash(HELPER,X2D_FLASH_HELPER_SHA,4096,0x100000))return fail("helper_hash");
    pulse_library=dlopen(HELPER,2);if(!pulse_library)return fail("helper_load");
    pulse_init=(int(*)(void))dlsym(pulse_library,"x2d_pulse_resident_init");
    pulse_close=(int(*)(void))dlsym(pulse_library,"x2d_pulse_resident_close");
    pulse_fire=(int(*)(PulseTiming*))dlsym(pulse_library,"x2d_pulse_resident_fire");
    return pulse_init&&pulse_close&&pulse_fire&&pulse_init()==0?1:fail("helper_init");
}
static int stop_sampling(void){
    flash_cancel(&core);sampling=0;
    /* 输出已经取消，停止时丢弃旧样本；仅 R1 记录器检查尾部样本计数。 */
    cleanup_mode(0);event_count=0;closed_events=0;total_samples=0;queued=0;
    thread_dirty=0;changed_count=0;sampled_threads=0;next_refresh_ns=0;
    if(mem_fd>=0){if(close(mem_fd)<0)fail("capture_mode_read_close");mem_fd=-1;}return !failure;
}
/* 故障暂停输出但保留用户开关；资源确认关闭后才允许重建采样。 */
static int sampling_fault(void){
    const char *reason=failure?failure:"sampling_unknown";int saved_errno=failure_errno;
    failure=0;failure_errno=0;
    stop_sampling();
    if(pulse_library){
        if(!pulse_close||pulse_close()!=0)fail("pulse_close_unconfirmed");
        else{dlclose(pulse_library);pulse_library=0;pulse_init=0;pulse_close=0;pulse_fire=0;}
    }
    sampling_retryable=!failure&&!equal(reason,"pulse_failed_no_retry")&&!equal(reason,"target_identity_changed")&&
        !equal(reason,"disable_failed")&&!equal(reason,"unmap_failed")&&!equal(reason,"close_failed")&&!equal(reason,"capture_mode_read_close");
    retry_sampling_ns=now()+1000000000ull;
    char log[256];snprintf(log,sizeof log,"SAMPLING_PAUSED_SWITCH_KEPT reason=%s errno=%d cleanup=%s retryable=%u requested=%u\n",reason,saved_errno,failure?failure:"closed",sampling_retryable,requested_enabled);
    sampling_error=reason;sampling_errno=saved_errno;failure=0;failure_errno=0;
    if(!output(log))return fail("sampling_log_write");
    page_status(&core,"sampling_disabled",allowed_mask,0,helper_ok,actual_ns);return 1;
}
static int has_tid(const unsigned *list,unsigned count,unsigned tid){
    for(unsigned i=0;i<count;i++)if(list[i]==tid)return 1;return 0;
}
static int mark_thread_change(unsigned tid){
    thread_dirty=1;
    if(has_tid(changed_tids,changed_count,tid))return 1;
    if(changed_count==MAX_THREADS)return fail("thread_change_limit");
    changed_tids[changed_count++]=tid;return 1;
}
static int remove_event(unsigned i){
    Event *e=&events[i];
    if(e->fd>=0&&ioctl(e->fd,PERF_DISABLE,0)<0)fail("disable_failed");
    if(e->mapping){if(munmap(e->mapping,e->length)<0)fail("unmap_failed");e->mapping=0;}
    if(e->fd>=0){if(close(e->fd)<0)fail("close_failed");e->fd=-1;}
    if(failure)return 0;
    --event_count;events[i]=events[event_count];polls[i]=polls[event_count];return 1;
}
static int remove_thread(unsigned tid){
    for(unsigned i=0;i<event_count;)if(events[i].tid==tid){if(!remove_event(i))return 0;}else ++i;
    return 1;
}
static void invalidate_cycle(void){
    if(core.pending||core.cycle_valid)++core.skipped;
    core.pending=0;core.cycle_valid=0;core.pair_wait=0;core.consumed=1;core.last_event_ns=0;
}
/* 同线程候选保留仍被完整覆盖的曝光线程；跨线程候选在覆盖变化时丢弃周期。 */
static int refresh_threads(void){
    if(core.source==5||core.source==6)invalidate_cycle();
    for(unsigned attempt=0;attempt<4;attempt++){
        uint64_t ticks;unsigned list[MAX_THREADS],count=0;
        if(!identity(&ticks)||ticks!=start_ticks)return fail("target_identity_changed");
        if(!threads(list,&count))return fail("sampling_thread_directory");
        if(core.cycle_valid&&(!has_tid(list,count,core.tid)||has_tid(changed_tids,changed_count,core.tid)))invalidate_cycle();
        for(unsigned i=0;i<event_count;){
            if(!has_tid(list,count,events[i].tid)||has_tid(changed_tids,changed_count,events[i].tid)){
                if(!remove_event(i))return 0;
            }else ++i;
        }
        changed_count=0;unsigned raced=0;
        for(unsigned i=0;i<count&&!raced;i++){
            unsigned existing=0;for(unsigned k=0;k<event_count;k++)if(events[k].tid==list[i])++existing;
            if(existing==active_site_count)continue;
            if(existing)return fail("thread_site_coverage");
            unsigned first=event_count;
            for(unsigned j=0;j<active_site_count;j++){
                if(!attach(list[i],active_sites[j],sampling_page)){
                    /* snapshot 后短命线程已结束：解除本线程部分附着，再重新核对。 */
                    if(equal(failure,"perf_event_open")&&failure_errno==3){
                        failure=0;failure_errno=0;if(!remove_thread(list[i]))return 0;raced=1;break;
                    }return 0;
                }
                if(ioctl(events[event_count-1].fd,PERF_ENABLE,0)<0)return fail("enable_failed");
            }
            if(!raced){
                uint64_t ready=now();if(!ready)return fail("clock_start");
                for(unsigned k=first;k<event_count;k++)events[k].ready_ns=ready;
            }
        }
        if(raced)continue;
        uint64_t boundary=now();if(!boundary)return fail("clock_start");
        if(!same_threads(list,count))continue;
        if(!identity(&ticks)||ticks!=start_ticks)return fail("target_identity_changed");
        for(unsigned i=0;i<count;i++)tids[i]=list[i];tid_count=count;sampled_threads=count;
        if(core.source==5||core.source==6)core.configured_ns=boundary;
        thread_dirty=0;next_refresh_ns=0;++thread_refreshes;
        char log[160];snprintf(log,sizeof log,"THREADS_SYNCHRONIZED threads=%u events=%u revision=%u preserved_cycle=%u\n",count,event_count,thread_refreshes,core.cycle_valid);
        return output(log)?1:fail("sampling_log_write");
    }
    /* 关闭页面时线程会连续退出，不能将四次快照变化解释成用户关闭开关。
     * 有界处理后回主循环，继续收取消指令；覆盖未稳定时丢弃周期，20 ms 后再跟随。 */
    thread_dirty=1;invalidate_cycle();core.configured_ns=now();
    if(!core.configured_ns)return fail("clock_start");
    next_refresh_ns=core.configured_ns+20000000ull;return 1;
}
static int start_sampling(unsigned source){
    unsigned *sites=active_sites,count=2;sites[0]=SITE_camx_entry;
    if(source==1||source==2)sites[1]=SITE_vsync_entry;
    else if(source==3){sites[1]=SITE_tg_entry;sites[2]=SITE_tg_written;count=3;}
    else if(source==4){sites[1]=SITE_channel_entry;sites[2]=SITE_channel_written;count=3;}
    else if(source==5)sites[1]=SITE_irq_dispatch;
    else if(source==6)sites[1]=SITE_sof;
    else if(source==7)sites[1]=SITE_camx_state2_publish;
    else return fail("candidate");
    if(!identity(&start_ticks))return fail("sampling_target_identity");
    if(!source_and_mapping_sites(sites,count))return 0;
    char path[80];snprintf(path,sizeof path,"/proc/%u/mem",target_pid);mem_fd=open(path,0x80000);
    if(mem_fd<0)return fail("capture_mode_read_open");
    int page=getpagesize();if(page<4096||page>65536||(page&(page-1)))return fail("page_size");
    active_site_count=count;sampling_page=(unsigned)page;
    if(!refresh_threads())return 0;
    core.configured_ns=now();if(!core.configured_ns)return fail("clock_start");sampling=1;return 1;
}
static int collect_ring(Event *e){
    unsigned char record[4096],*m=e->mapping;
    uint64_t head=__atomic_load_n((uint64_t*)(m+1024),__ATOMIC_ACQUIRE);
    while(e->tail!=head){
        size_t length=0;SensorRecord r;
        if(sensor_ring_copy(m+e->data_offset,e->data_size,head,e->tail,record,&length)!=1)return fail("ring_overrun_or_partial");
        int parsed=sensor_record(record,length,&r);
        if(parsed==3){
            /* 独立子进程不会执行目标进程的断点，不误判为目标线程变化。 */
            e->tail+=r.size;__atomic_store_n((uint64_t*)(m+1032),e->tail,__ATOMIC_RELEASE);
            if(r.pid!=target_pid||r.time_ns<=core.configured_ns)continue;
            if(!mark_thread_change(r.tid))return 0;continue;
        }
        if(parsed!=1)return fail(parsed==2?"lost_samples":parsed==3?"task_changed":"sample_abi");
        if(r.pid!=target_pid||r.tid!=e->tid||r.ip!=bias+sensor_sites[e->site].elf_va||r.time_ns<e->last_time)return fail("sample_identity_or_order");
        if(queued==MAX_SAMPLES)return fail("batch_limit");
        uint64_t timestamp=now();if(!timestamp||r.time_ns>timestamp)return fail("sample_clock");
        if(r.time_ns<=e->ready_ns||has_tid(changed_tids,changed_count,e->tid)){
            e->tail+=r.size;__atomic_store_n((uint64_t*)(m+1032),e->tail,__ATOMIC_RELEASE);continue;
        }
        FlashEvent f={.time_ns=r.time_ns,.tid=r.tid};unsigned site=e->site;
        (void)known_rva(r.regs[5],&f.caller);
        if(site==SITE_camx_entry){
            f.type=F_CAPTURE;f.eshutter=2;
            /* 只读本次 Expo+0x69 的 UserEShutter 字节，不保存结构地址或内存。 */
            if(r.time_ns>core.configured_ns){
                uint64_t p=r.regs[0];unsigned char mode=2;
                if(p<4096||p>=(1ull<<48)-0x6a||pread(mem_fd,&mode,1,(int64_t)(p+0x69))!=1)return fail("capture_mode_read");
                f.eshutter=mode;
            }
        }else if(site==SITE_vsync_entry){f.type=F_VSYNC;f.channel=(unsigned)r.regs[0];}
        else if(site==SITE_tg_entry){f.type=F_TG_ENTRY;f.value=(unsigned)r.regs[0];}
        else if(site==SITE_tg_written){f.type=F_TG_WRITTEN;f.value=(unsigned)r.regs[3];}
        else if(site==SITE_channel_entry){f.type=F_CH_ENTRY;f.value=(unsigned)r.regs[1];f.channel=(unsigned)r.regs[0];}
        else if(site==SITE_channel_written){f.type=F_CH_WRITTEN;f.value=(unsigned)r.regs[3];f.channel=(unsigned)r.regs[4];}
        else if(site==SITE_irq_dispatch){f.type=F_IRQ;f.value=(unsigned)r.regs[0];}
        else if(site==SITE_sof)f.type=F_SOF;
        else if(site==SITE_camx_state2_publish)f.type=F_STATE2;
        else return fail("site_unknown");
        queue[queued++]=(QueuedFlash){f,e->ready_ns};e->hits++;e->last_time=r.time_ns;e->tail+=r.size;
        __atomic_store_n((uint64_t*)(m+1032),e->tail,__ATOMIC_RELEASE);
    }return 1;
}
static int compare_event(const void *a,const void *b){
    const QueuedFlash *x=a,*y=b;return x->event.time_ns<y->event.time_ns?-1:x->event.time_ns>y->event.time_ns?1:0;
}
static int collect_events(void){
    queued=0;
    for(unsigned i=0;i<event_count;i++)if(!collect_ring(&events[i]))return 0;
    if(thread_dirty&&now()>=next_refresh_ns&&!refresh_threads())return 0;
    if(thread_dirty){invalidate_cycle();queued=0;return 1;}
    qsort(queue,queued,sizeof queue[0],compare_event);
    for(unsigned i=0;i<queued;i++){
        unsigned covered=0;for(unsigned k=0;k<event_count;k++)
            if(events[k].tid==queue[i].event.tid&&events[k].ready_ns==queue[i].coverage_ns){covered=1;break;}
        if(covered&&flash_event(&core,&queue[i].event)<0)return fail("ambiguous_event_order_or_pair");
    }
    return 1;
}
static int command_line(const char *line){
    int visible=front_console(line);if(visible<0)return fail("front_ack");if(visible)return 1;
    const char *p=front_marker(line,"X2D_SENSOR_FLASH 1 ");if(!p)return 1;p-=19;
    /* marker 长度固定 19 字节；prefix 不来自用户地址。 */
    int action=flash_command(&core,now(),p);
    if(action<0)return fail("invalid_command");
    if(action==3){last_ping_ns=now();return 1;}
    if(action==1||action==2){
        unsigned enabled=core.enabled;
        requested_enabled=enabled;
        if(!stop_sampling())return sampling_fault();
        if(sampling_error){
            if(!enabled||!sampling_retryable){page_status(&core,"sampling_disabled",allowed_mask,0,helper_ok,actual_ns);return 1;}
            sampling_error=0;sampling_errno=0;sampling_retryable=0;
        }
        if(enabled){
            if(!(allowed_mask&(1u<<(core.source-1))))return fail("trial_not_enabled");
            if(!pulse_library&&!resident_init())return sampling_fault();
            if(!start_sampling(core.source))return sampling_fault();
            core.enabled=1;
        }
        page_status(&core,"configured",allowed_mask,sampling,helper_ok,actual_ns);
    }return 1;
}
static int recover_sampling(void){
    if(!requested_enabled||sampling||!sampling_error||!sampling_retryable||now()<retry_sampling_ns)return 1;
    sampling_error=0;sampling_errno=0;sampling_retryable=0;
    if(!pulse_library&&!resident_init())return sampling_fault();
    if(!start_sampling(core.source))return sampling_fault();
    core.enabled=1;return output("SAMPLING_RECOVERED_SWITCH_KEPT_AWAIT_NEW_CAPTURE\n");
}
static char command_buffer[1024];static unsigned command_used;
static int consume_commands(int fd){
    char bytes[2048];unsigned consumed=0;
    for(;;){
        long n=read(fd,bytes,sizeof bytes);if(n<0&&*__errno()==4)continue;if(n<0)return fail("page_log_read");if(!n)return 1;
        if((consumed+=(unsigned)n)>32768)return fail("page_log_backlog");
        for(long i=0;i<n;i++){
            if(bytes[i]=='\n'){
                command_buffer[command_used]=0;
                if(!command_line(command_buffer))return 0;command_used=0;
            }else{
                if(command_used==sizeof command_buffer-1)return fail("page_log_line_limit");
                command_buffer[command_used++]=bytes[i];
            }
        }
    }
}
static int worker(void){
    if(!number(getenv("X2D_SENSOR_PID"),&target_pid)||!target_pid||
       !number(getenv("X2D_FLASH_VERIFIED_SOURCES"),&allowed_mask)||allowed_mask>127)return fail("arguments");
    if(!file_hash("/system/bin/camera-service","fbcf828f73bca13f0c8b95e7dd0b95ac483ae36954ec06179098c8a1a65f9f82",0x100000,0x4000000))return fail("service_hash");
    int input=page_initialize();if(input<0)return fail("page_identity");
    if(front_init()<0){close(input);return fail("front_init");}
    core.source=1;core.consumed=1;
    uint64_t began=now(),next_status=0,next_check=0,last=began;last_ping_ns=began;
    signal(2,on_signal);signal(15,on_signal);
    while(!interrupted&&!failure){
        uint64_t t=now();if(!t||t<last){fail("clock_backward");break;}last=t;
        int stop=open(PAGE_DIR "stop",0x20000);if(stop>=0){close(stop);break;}
        if(front_poll()<0){fail("front_identity_or_timeout");break;}
        if(!consume_commands(input))break;
        if(t>=next_check){
            uint64_t ticks;
            if(!page_identity()){fail("page_identity_changed");break;}
            if(sampling&&(!identity(&ticks)||ticks!=start_ticks)){fail("target_identity_changed");if(!sampling_fault())break;}
            if(sampling&&!same_threads(tids,tid_count))thread_dirty=1;
            next_check=t+250000000ull;
        }
        /* 页面是否渲染/发心跳不决定试闪开关；进程身份仍独立检查。 */
        if(!recover_sampling())break;
        if(sampling&&!collect_events()){if(!sampling_fault())break;}
        /* 本批所有环先核对丢样，再处理取消，再做输出。 */
        if(!consume_commands(input))break;
        if(bridge_failed){fail("status_write");break;}
        if(sampling&&!thread_dirty&&!command_used&&!interrupted&&flash_due(&core,now())){
            PulseTiming timing={0};helper_ok=pulse_fire&&pulse_fire(&timing)==0;
            actual_ns=timing.before_high_us*1000;
            if(actual_ns>=core.deadline_ns)core.late_ns=actual_ns-core.deadline_ns;
            char log[384];snprintf(log,sizeof log,"PULSE C%u cycle=%u delay_us=%u event_ns=%llu target_ns=%llu before_high_us=%llu after_high_us=%llu after_low_us=%llu result=%u\n",
                core.source,core.cycle,core.delay_us,(unsigned long long)core.event_ns,(unsigned long long)core.deadline_ns,
                (unsigned long long)timing.before_high_us,(unsigned long long)timing.after_high_us,(unsigned long long)timing.after_low_us,helper_ok);
            if(!output(log)){fail("pulse_log_write");break;}
            if(!helper_ok){fail("pulse_failed_no_retry");if(!sampling_fault())break;}
        }
        if(t>=next_status){page_status(&core,sampling_error?"sampling_disabled":thread_dirty?"sampling_wait_threads":flash_ui_stale(t,last_ping_ns)?"page_status_stale":"ready",allowed_mask,sampling,helper_ok,actual_ns);next_status=t+100000000ull;}
        if(bridge_failed){fail("status_write");break;}
        /* 按剩余纳秒等待，避免把微秒延迟主动向上取整为 1 ms。
         * 仍受内核调度影响；最多 1 ms 返回检查页面命令。 */
        uint64_t wait_ns=1000000ull,wait_now=now();
        if(!wait_now){fail("clock_wait");break;}
        if(core.pending){
            uint64_t remain=core.deadline_ns>wait_now?core.deadline_ns-wait_now:0;
            if(remain<wait_ns)wait_ns=remain;
        }
        TimeSpec timeout={0,(int64_t)wait_ns};
        int p=ppoll(polls,event_count,&timeout,0);
        if(p<0&&*__errno()!=4){fail("poll_failed");break;}
        for(unsigned i=0;i<event_count;i++){
            if(polls[i].revents&(8|32)){fail("event_unavailable");sampling_fault();break;}
            if((polls[i].revents&16)&&!mark_thread_change(events[i].tid)){sampling_fault();break;}
        }
    }
    close(input);if(front_fd>=0)close(front_fd);
    return !failure;
}
#ifndef X2D_FLASH_WORKER_TEST
__attribute__((constructor)) static void start_flash(void){
    if(!equal(getenv("X2D_SENSOR_FLASH_UI"),"4.2.0")||!is_true_process())return;
    int ok=worker();stop_sampling();
    if(pulse_library){if(!pulse_close||pulse_close()!=0)fail("pulse_close_unconfirmed");else dlclose(pulse_library);}
    flash_cancel(&core);
    page_status(&core,failure?failure:"worker_exit_disabled",allowed_mask,0,helper_ok,actual_ns);
    if(failure){output("SENSOR_FLASH_STOP ");output(failure);output("\n");}
    _exit(ok&&!failure?0:20);
}
#endif
