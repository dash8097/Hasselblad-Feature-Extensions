#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#ifdef X2D_PULSE_KERNEL_SCHED
#include <sys/syscall.h>
extern long syscall(long,...);
#endif
#define X2D_RESIDENT_UNIT 1
#include "native_hotshoe_resident.c"
static PulseGuard test_guard;
static uint32_t test_reg;
static uint64_t ticks;
static unsigned checks,highs,lows,sleeps,clocks;
static int reap,owner_lost,stuck_low,watch_run,stale_idle,bad_clock,stale_errno;
static int policy,priority,enter_fail,restore_fail,verify_bad,ordinary_preempt,expected_exit,allow_ordinary_baseline;
static unsigned critical_highs;
static jmp_buf escape;
#define C(v) do{++checks;if(!(v)){printf("NATIVE_PULSE_FAILED check=%u line=%u\n",checks,(unsigned)__LINE__);exit(1);}}while(0)
int *test_errno(void){return &stale_errno;}
int test_clock_gettime(int id,void*out){(void)id;PulseTime*t=out;++clocks;ticks+=10;
 if(stale_idle&&clocks==2){test_guard.active=0;test_reg&=~1u;test_guard.stop=1;ticks+=20000;}
 if(ordinary_preempt&&(test_reg&1u)&&policy==0){ticks+=20000;test_guard.fault=1;}
 if(bad_clock)return -1;
 t->sec=(long)(ticks/1000000);t->nsec=(long)((ticks%1000000)*1000);return 0;}
int test_waitpid(int pid,int*status,int options){(void)options;*status=0;return reap?pid:0;}
int test_getpid(void){return 10;}
int test_sched_getscheduler(int pid){C(pid==0);
#ifdef X2D_PULSE_KERNEL_SCHED
 return (int)syscall(SYS_sched_getscheduler,pid);
#else
 return policy==1&&verify_bad?0:policy;
#endif
}
int test_sched_getparam(int pid,PulseSchedule*p){C(pid==0);
#ifdef X2D_PULSE_KERNEL_SCHED
 return (int)syscall(SYS_sched_getparam,pid,p);
#else
 p->priority=priority;return 0;
#endif
}
int test_sched_setscheduler(int pid,int wanted,const PulseSchedule*p){
 C(pid==0&&(wanted==0||wanted==1));
 if(wanted==1&&enter_fail){stale_errno=13;return -1;}
 if(wanted==0&&restore_fail){stale_errno=1;return -1;}
 C(p->priority==(wanted==1?1:0));
#ifdef X2D_PULSE_KERNEL_SCHED
 int result=(int)syscall(SYS_sched_setscheduler,pid,wanted,p);if(result)return result;
#endif
 policy=wanted;priority=p->priority;return 0;
}
int test_getppid(void){return owner_lost?1:10;}
int test_fork(void){return -1;}
int test_poll(void*p,unsigned long n,int timeout){(void)p;(void)n;ticks+=(unsigned)timeout*1000;return 0;}
void *test_dlopen(const char*p,int f){(void)p;(void)f;return 0;}
void *test_dlsym(void*p,const char*n){(void)p;(void)n;return 0;}
int test_dlclose(void*p){(void)p;return 0;}
void *test_mmap(void*p,unsigned long n,int a,int b,int c,long d){(void)p;(void)n;(void)a;(void)b;(void)c;(void)d;return (void*)-1;}
int test_munmap(void*p,unsigned long n){(void)p;(void)n;return 0;}
void test_exit(int status){C(status==expected_exit);longjmp(escape,1);}
static void native_deinit(void){}
static void native_set(int high){if(high){C((policy==1&&priority==1)||allow_ordinary_baseline);if(policy==1)++critical_highs;++highs;test_reg|=1;}else{++lows;if(!stuck_low)test_reg&=~1u;}}
static void native_sleep(unsigned us){ticks+=us;++sleeps;if(watch_run){ticks+=10000;if(sleeps>=2)store_flag(&test_guard.stop,1);}}
static void reset(void){memset(&test_guard,0,sizeof test_guard);test_reg=0x20;ticks=100;highs=lows=sleeps=clocks=critical_highs=0;reap=owner_lost=stuck_low=watch_run=stale_idle=bad_clock=0;policy=priority=enter_fail=restore_fail=verify_bad=ordinary_preempt=expected_exit=allow_ordinary_baseline=0;stale_errno=11;resident_guard=&test_guard;resident_reg=&test_reg;resident_set=native_set;resident_sleep=native_sleep;resident_deinit=native_deinit;guard_pid=123;resident_library=0;}
static int stale_guard(void){reset();test_guard.active=1;test_reg|=1;stale_idle=1;if(!setjmp(escape))guard_loop(10);printf("STALE_IDLE_GUARD fault=%u low_writes=%u\n",test_guard.fault,lows);return test_guard.fault||lows?1:0;}
int main(int argc,char**argv){if(argc>1){
 if(!strcmp(argv[1],"ordinary-window")){
  reset();ordinary_preempt=allow_ordinary_baseline=1;PulseTiming timing={0};int rc=x2d_pulse_resident_fire(&timing);
  printf("ORDINARY_WINDOW result=%d fault=%u errno=%d width_us=%llu restored_policy=%d\n",rc,test_guard.fault,stale_errno,(unsigned long long)(timing.after_low_us-timing.after_high_us),policy);
  return rc<0&&test_guard.fault&&stale_errno==110?1:rc==0&&!test_guard.fault&&policy==0?0:2;
 }
 return stale_guard();
 }
 PulseTiming t;C(!stale_guard());
 reset();C(x2d_pulse_resident_init()==-1&&!highs);
 reset();for(unsigned i=0;i<256;++i){C(!x2d_pulse_resident_fire(&t));C(test_reg==0x20&&!test_guard.active);C(t.before_high_us<t.after_high_us&&t.after_high_us<t.after_low_us);C(t.after_low_us-t.after_high_us>=1000&&t.after_low_us-t.after_high_us<1500);C(policy==0&&priority==0);}C(highs==256&&critical_highs==256&&lows==256&&sleeps==0);
 reset();ordinary_preempt=1;C(!x2d_pulse_resident_fire(&t)&&!test_guard.fault&&policy==0&&critical_highs==1);
 reset();enter_fail=1;C(x2d_pulse_resident_fire(&t)<0&&!highs&&policy==0&&stale_errno==13);
 reset();policy=2;C(x2d_pulse_resident_fire(&t)<0&&!highs&&policy==2&&stale_errno==16);
 reset();verify_bad=1;C(x2d_pulse_resident_fire(&t)<0&&!highs&&policy==0&&stale_errno==5);
 reset();restore_fail=1;expected_exit=124;if(!setjmp(escape)){x2d_pulse_resident_fire(&t);C(0);}C(highs==1&&lows==1&&test_reg==0x20&&!test_guard.active);
 reset();reap=1;C(x2d_pulse_resident_fire(&t)<0&&!highs&&stale_errno==10);
 reset();test_reg|=1;C(x2d_pulse_resident_fire(&t)<0&&!highs&&stale_errno==16);
 reset();test_guard.fault=1;C(x2d_pulse_resident_fire(&t)<0&&!highs&&stale_errno==110);
 reset();test_guard.active=1;C(x2d_pulse_resident_fire(&t)<0&&!highs&&stale_errno==16);
 reset();test_guard.stop=1;C(x2d_pulse_resident_fire(&t)<0&&!highs&&stale_errno==125);
 reset();bad_clock=1;C(x2d_pulse_resident_fire(&t)<0&&!highs&&stale_errno==22);
 reset();stuck_low=1;C(x2d_pulse_resident_fire(&t)<0&&test_guard.active&&lows==2&&stale_errno==5);
 reset();test_guard.active=1;test_reg|=1;reap=1;C(!x2d_pulse_resident_close()&&test_reg==0x20&&!resident_guard&&!guard_pid);
 reset();owner_lost=1;test_reg|=1;test_guard.active=1;if(!setjmp(escape))guard_loop(10);C(lows==1&&test_reg==0x20&&test_guard.fault);
 reset();owner_lost=1;test_reg|=1;if(!setjmp(escape))guard_loop(10);C(!lows);
 reset();test_guard.active=1;test_reg|=1;watch_run=1;if(!setjmp(escape))guard_loop(10);C(lows==1&&test_guard.fault&&test_reg==0x20);
 printf("NATIVE_PULSE_PASSED assertions=%u mocked_hardware=1\n",checks);return 0;
}
