/* Fixed X2D 4.2.0 native map/set functions; the original inspect constructor
 * is retained. No pulse is emitted on load or initialize. */
#ifdef X2D_RESIDENT_UNIT
#include "resident_test_platform.h"
#else
#include "../hotshoe_once.c"
#endif
extern int clock_gettime(int,void *);
extern int poll(void *,unsigned long,int);
typedef struct { long sec,nsec; } PulseTime;
typedef struct { volatile unsigned ready,stop,active,fault; } PulseGuard;
typedef struct { uint64_t before_high_us,after_high_us,after_low_us; } PulseTiming;
static void *resident_library;
static volatile uint32_t *resident_reg;
static PulseGuard *resident_guard;
static set_fn resident_set;
static sleep_fn resident_sleep;
static deinit_fn resident_deinit;
static int guard_pid,guard_status;
static uint64_t pulse_us(void) {
    PulseTime t;if(clock_gettime(1,&t)||t.sec<0||t.nsec<0)return 0;
    return (uint64_t)t.sec*1000000u+(uint64_t)t.nsec/1000u;
}
static unsigned load_flag(volatile unsigned *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void store_flag(volatile unsigned *p,unsigned v){__atomic_store_n(p,v,__ATOMIC_RELEASE);}
/* Idle guardian never writes the register. It only lowers an output owned by
 * this worker after owner loss or a prolonged observed high. Linux scheduling
 * is not a hardware deadline guarantee. */
static void guard_loop(int owner) {
    uint64_t high_since=0;
    store_flag(&resident_guard->ready,1);
    for(;;){
        unsigned active=load_flag(&resident_guard->active);
        int exiting=load_flag(&resident_guard->stop)||getppid()!=owner;
        if(active && (*resident_reg&1u)){
            uint64_t now=pulse_us();
            if(!high_since)high_since=now;
            if(exiting||!now||now<high_since||now-high_since>=10000){
                resident_set(0);store_flag(&resident_guard->fault,1);
            }
        } else high_since=0;
        if(exiting)break;
        resident_sleep(active?100:1000);
    }
    resident_deinit();munmap(resident_guard,4096);_exit(0);
}
__attribute__((visibility("default"))) int x2d_pulse_resident_close(void) {
    int ok=1;
    if(resident_guard){
        if(load_flag(&resident_guard->active)){
            resident_set(0);
            if(*resident_reg&0x80000001u)ok=0;
            else store_flag(&resident_guard->active,0);
        }
        store_flag(&resident_guard->stop,1);
        if(guard_pid){
            int reaped=0;
            for(unsigned i=0;i<200;++i){
                int r=waitpid(guard_pid,&guard_status,1);
                if(r==guard_pid){reaped=1;break;}
                if(r<0)break;
                poll(0,0,5);
            }
            if(!reaped)return -1; /* keep mapping/library until guardian ends */
            guard_pid=0;if(guard_status)ok=0;
        }
        munmap(resident_guard,4096);resident_guard=0;
    }
    if(resident_deinit){resident_deinit();resident_deinit=0;}
    if(resident_library){dlclose(resident_library);resident_library=0;}
    resident_reg=0;return ok?0:-1;
}
__attribute__((visibility("default"))) int x2d_pulse_resident_init(void) {
    if(resident_library||!selected_process()||!verified_binary())return -1;
    resident_library=dlopen("/system/lib64/librcam.so",2);
    if(!resident_library)return -1;
    product_fn product=(product_fn)dlsym(resident_library,"producttype_get_product_id");
    if(!product||product()!=4)goto failed;
    uintptr_t bias=(uintptr_t)product-0x384f8;
    NativeMap *maps=(NativeMap*)(bias+0x2fa878);
    init_fn init=(init_fn)(bias+0x1afc10);
    addr_fn addr=(addr_fn)(bias+0x1afe78);
    resident_deinit=(deinit_fn)(bias+0x1afde8);
    resident_set=(set_fn)(bias+0xe2be0);
    resident_sleep=(sleep_fn)(bias+0x1b08e8);
    if(init()!=0||!mapped_ok(maps))goto failed;
    resident_reg=addr(0,0xf330b01c);
    if((uintptr_t)resident_reg!=maps[0].mapped+(0xf330b01c-maps[0].physical)||
       (*resident_reg&0x80000001u))goto failed;
    resident_guard=mmap(0,4096,3,0x21,-1,0);
    if(resident_guard==(void*)-1){resident_guard=0;goto failed;}
    store_flag(&resident_guard->ready,0);store_flag(&resident_guard->stop,0);
    store_flag(&resident_guard->active,0);store_flag(&resident_guard->fault,0);
    int owner=getpid();guard_pid=fork();
    if(guard_pid==0)guard_loop(owner);
    if(guard_pid<0){guard_pid=0;goto failed;}
    for(unsigned i=0;i<200;++i){
        if(load_flag(&resident_guard->ready))return 0;
        poll(0,0,1);
    }
failed:
    x2d_pulse_resident_close();return -1;
}
__attribute__((visibility("default"))) int x2d_pulse_resident_fire(PulseTiming *timing) {
    if(!timing||!resident_guard||!guard_pid||load_flag(&resident_guard->fault)||
       load_flag(&resident_guard->active)||load_flag(&resident_guard->stop))return -1;
    int got=waitpid(guard_pid,&guard_status,1);
    if(got!=0){if(got==guard_pid)guard_pid=0;return -1;}
    if(*resident_reg&0x80000001u)return -1;
    timing->before_high_us=pulse_us();
    if(!timing->before_high_us)return -1;
    store_flag(&resident_guard->active,1);
    resident_set(1);timing->after_high_us=pulse_us();
    int high_seen=!!(*resident_reg&1u);
    resident_sleep(1000);
    resident_set(0);timing->after_low_us=pulse_us();
    if(*resident_reg&0x80000001u){resident_set(0);if(*resident_reg&0x80000001u)return -1;}
    store_flag(&resident_guard->active,0);
    return high_seen&&!load_flag(&resident_guard->fault)?0:-1;
}
