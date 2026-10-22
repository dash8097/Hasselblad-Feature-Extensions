/* 实际ARM Linux调度系统调用；硬件寄存器/时钟仍为显式替身。 */
#include <sys/reboot.h>
#define X2D_PULSE_KERNEL_SCHED 1
#define main unused_mock_main
#include "test_native_pulse.c"
#undef main
int main(void){
 reset();PulseTiming t;
 for(unsigned i=0;i<64;i++){
  C(test_sched_getscheduler(0)==0);
  C(x2d_pulse_resident_fire(&t)==0);
  C(test_sched_getscheduler(0)==0&&policy==0&&priority==0);
  C(test_reg==0x20&&!test_guard.active);
 }
 C(highs==64&&critical_highs==64&&lows==64);
 printf("NATIVE_PULSE_KERNEL_SCHED_PASSED cycles=64 assertions=%u\n",checks);
 fflush(stdout);reboot(RB_POWER_OFF);return 0;
}
