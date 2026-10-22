#include "native_wait_policy.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef X2D_WAIT_LINUX
#include <poll.h>
#include <unistd.h>
#include <sys/reboot.h>
#endif
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("NATIVE_WAIT_FAILED line=%u\n",__LINE__);exit(1);}}while(0)
int main(void){
    CHECK(native_flash_wait_ns(1,0,0,100)==10000000ull);
    CHECK(native_flash_wait_ns(0,0,0,100)==50000000ull);
    CHECK(native_flash_wait_ns(1,1,100,100)==0);
    CHECK(native_flash_wait_ns(1,1,99,100)==0);
    const uint64_t intervals[]={1,100,999,1000,999999,1000000,9999999,10000000,10000001,49999999,50000000,50000001,160000000,370000000};
    for(unsigned sampling=0;sampling<2;++sampling)for(unsigned i=0;i<sizeof intervals/sizeof intervals[0];++i){
        uint64_t delay=intervals[i],value=native_flash_wait_ns(sampling,1,100+delay,100);
        CHECK(value<=delay);CHECK(value<=(sampling?10000000ull:50000000ull));
        if(delay<10000000ull)CHECK(value==delay);
    }
    CHECK(native_flash_wait_ns(1,1,UINT64_MAX,UINT64_MAX-1)==1);
    /* Count idle wakeups over the same one-second budget; no camera timing is
     * inferred from this deterministic scheduling-policy test. */
    unsigned wakeups=0;for(uint64_t t=0;t<1000000000ull;t+=native_flash_wait_ns(1,0,0,t))++wakeups;
    CHECK(wakeups==100);
#ifdef X2D_WAIT_LINUX
    CHECK(getpid()==1);
    int fds[2];CHECK(pipe(fds)==0);CHECK(write(fds[1],"x",1)==1);
    struct pollfd fd={fds[0],POLLIN,0};struct timespec budget={0,(long)native_flash_wait_ns(1,0,0,1)};
    CHECK(ppoll(&fd,1,&budget,0)==1);CHECK(fd.revents&POLLIN);
    char value;CHECK(read(fds[0],&value,1)==1&&value=='x');
    budget.tv_nsec=(long)native_flash_wait_ns(1,1,200100,100);
    CHECK(budget.tv_nsec==200000);CHECK(ppoll(&fd,1,&budget,0)==0);
    CHECK(close(fds[0])==0&&close(fds[1])==0);
    printf("NATIVE_WAIT_PASSED checks=%u kernel_poll=1\n",checks);fflush(stdout);reboot(RB_POWER_OFF);
#else
    printf("NATIVE_WAIT_PASSED checks=%u kernel_poll=0\n",checks);
#endif
    return 0;
}
