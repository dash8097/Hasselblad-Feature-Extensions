#include "esflash_timing.hpp"
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned checks,calls,lastProfile,lastDelay;static bool accept=true;
static void check(bool b,const char*n){++checks;if(!b){std::fprintf(stderr,"FLASH_TIMING_FAIL %s\n",n);std::exit(1);}}
extern "C" int x2d_flash_client_set(X2dFlashClient*c,unsigned profile,unsigned delay,uint64_t){
 if(!accept||!c->ready||c->pending||c->queued)return 0;
 ++calls;lastProfile=profile;lastDelay=delay;c->pending=1;c->pending_operation=1;return 1;
}
static void ack(X2dFlashClient&c){c.pending=c.pending_operation=0;c.state.profile=lastProfile;c.state.delay_us=lastDelay;c.state.requested=lastProfile!=0;c.state.fault=0;}
int main(){
 FlashExposure e{true,true,true,0,28};FlashTiming f;X2dFlashClient c{};
 check(e.profile()==1,"14-1/5-normal");e.tv=32;check(e.profile()==2,"14-1/6-fast");e.tv=120;check(e.profile()==2,"14-1/1000-fast");
 e.depth=1;e.tv=16;check(e.profile()==3,"16-0.4-normal");e.tv=20;check(e.profile()==4,"16-0.3-fast");
 check(x2d_flash_default_delay(1)==168000&&x2d_flash_default_delay(2)==160000&&x2d_flash_default_delay(3)==370000&&x2d_flash_default_delay(4)==370000,"measured-delays");
 f.tick(c,e,0);check(!calls,"unavailable-service-does-not-send");c.ready=1;
 f.tick(c,e,1);check(calls==1&&lastProfile==4&&lastDelay==370000,"electronic-startup-auto-arms-without-menu-or-switch");ack(c);
 for(unsigned n=0;n<100;n++)f.tick(c,e,n+2);check(calls==1,"no-repeated-set-no-menu-required");
 e.depth=0;e.tv=28;f.tick(c,e,200);check(lastProfile==1&&lastDelay==168000&&calls==2,"bit-depth-change-reselects");ack(c);
 e.tv=32;e.idle=false;f.tick(c,e,201);check(calls==2,"busy-exposure-retains-profile");e.idle=true;f.tick(c,e,202);check(lastProfile==2&&calls==3,"idle-applies-new-boundary");ack(c);
 e.electronic=false;f.tick(c,e,203);check(!lastProfile&&calls==4,"mechanical-disarms-extra-pulse");ack(c);f.tick(c,e,204);check(calls==4,"mechanical-does-not-repeat-off");
 e.electronic=true;f.tick(c,e,205);check(lastProfile==2&&calls==5,"electronic-resumes-automatically");ack(c);
 e.valid=false;f.tick(c,e,206);check(!lastProfile&&calls==6,"invalid-setting-disarms-no-guess");ack(c);f.tick(c,e,207);check(calls==6,"invalid-snapshot-no-repeat");
 e.valid=true;f.tick(c,e,208);check(lastProfile==2&&calls==7,"valid-snapshot-auto-resumes");ack(c);
 c.state.fault=1;f.tick(c,e,209);check(calls==7,"service-fault-not-spammed");c.state.fault=0;c.state.requested=0;c.state.profile=0;f.tick(c,e,210);check(calls==8&&lastProfile==2,"recovered-service-restores-always-on-policy");ack(c);
 e.depth=1;c.pending=1;c.pending_operation=0;f.tick(c,e,211);check(calls==8,"in-flight-read-not-overridden");c.pending=0;f.tick(c,e,212);check(calls==9&&lastProfile==4,"latest-settings-after-read");
 c.ready=0;c.pending=0;unsigned count=calls;f.tick(c,e,213);check(calls==count,"transport-loss-no-send");c.ready=1;ack(c);f.tick(c,e,214);check(calls==count,"applied-lost-reply-not-duplicated");
 c.state.requested=0;c.state.profile=0;f.tick(c,e,215);check(calls==count+1,"confirmed-off-reconciles-to-user-always-on-policy");ack(c);
 accept=false;e.tv=16;f.tick(c,e,216);check(calls==count+1,"unsent-change-not-applied");accept=true;f.tick(c,e,217);check(calls==count+2&&lastProfile==3,"unsent-change-retried");ack(c);
 unsigned faultCalls=calls;c.state.fault=1;e.electronic=false;
 f.tick(c,e,220);check(calls==faultCalls+1&&lastProfile==0,"fault-still-accepts-mechanical-off");ack(c);c.state.fault=1;
 f.tick(c,e,221);check(calls==faultCalls+1,"fault-off-not-repeated");e.electronic=true;
 f.tick(c,e,222);check(calls==faultCalls+2&&lastProfile==3,"fresh-electronic-setting-after-fault-off");ack(c);c.state.fault=1;e.tv=20;
 f.tick(c,e,223);check(calls==faultCalls+2,"requested-fault-not-reconfigured-or-spammed");
 std::printf("ESFLASH_TIMING_PASS assertions=%u camera_requests=0 lamp_mode_commands=0\n",checks);
}
