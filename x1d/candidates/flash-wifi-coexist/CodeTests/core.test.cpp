#include "../native/coexist_core.h"
#include "../native/link_evidence.h"
#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>
using namespace coexist;
static unsigned checks=0;
static void check(bool ok) {++checks;if(!ok){fprintf(stderr,"failed check %u\n",checks);exit(1);}}
static Command next(Core &c,Action action) {auto r=c.take();check(r.action==action);check(c.take().action==Action::None);return r;}
static void done(Core &c,Action action,bool ok=true) {auto r=next(c,action);check(c.complete(r,ok,100));check(!c.complete(r,ok,101));}
static void setup(Core &c) {check(c.enable(true));c.networkStatus(true);check(c.press(1,1));}
int main() {
    check(Channel==5 && Id==5 && GroupD==3 && PowerTenths==50);
    {
        Core c;check(!c.press(1,1));check(c.enable(true));check(!c.press(1,1));c.networkStatus(true);
        check(c.press(1,1));check(!c.press(2,2));done(c,Action::SaveNetwork);done(c,Action::OpenFlash);
        done(c,Action::SendPower);check(!c.exposureAllowed);done(c,Action::ContinueExposure);
        check(c.exposureAllowed);check(!c.shotEnded(2,500));check(c.shotEnded(1,500));
        done(c,Action::CloseFlash);check(c.radioReleased);done(c,Action::RestoreNetwork);
        check(c.phase==Phase::Network);check(!c.press(1,600));check(c.press(2,600));
    }
    {
        Core c;setup(c);auto old=next(c,Action::SaveNetwork);check(c.cancel(10));
        check(c.complete(old,true,11));done(c,Action::RestoreNetwork);check(!c.exposureAllowed);
        check(c.phase==Phase::Network);
    }
    for(Action failure: {Action::OpenFlash,Action::SendPower}) {
        Core c;setup(c);done(c,Action::SaveNetwork);
        if(failure==Action::SendPower) done(c,Action::OpenFlash);
        done(c,failure,false);done(c,Action::CloseFlash);done(c,Action::RestoreNetwork);
        check(!c.exposureAllowed && c.phase==Phase::Network);
    }
    {
        Core c;setup(c);done(c,Action::SaveNetwork);done(c,Action::OpenFlash,false);
        done(c,Action::CloseFlash,false);check(c.phase==Phase::Fault && !c.radioReleased);
        check(c.take().action==Action::None);check(!c.press(2,800));
    }
    {
        Core c(20);setup(c);done(c,Action::SaveNetwork);auto late=next(c,Action::OpenFlash);
        check(c.tick(200));check(c.pendingUnknown());check(c.take().action==Action::None);
        check(c.complete(late,true,201));done(c,Action::CloseFlash);done(c,Action::RestoreNetwork);
        check(!c.exposureAllowed);check(c.phase==Phase::Network);
    }
    {
        Core c;setup(c);done(c,Action::SaveNetwork);done(c,Action::OpenFlash);done(c,Action::SendPower);
        auto late=next(c,Action::ContinueExposure);check(c.cancel(20));
        check(c.complete(late,true,21));done(c,Action::CancelShot);done(c,Action::CloseFlash);
        done(c,Action::RestoreNetwork);check(!c.exposureAllowed);
    }
    {
        Core c;setup(c);done(c,Action::SaveNetwork);done(c,Action::OpenFlash,false);
        done(c,Action::CloseFlash);done(c,Action::RestoreNetwork,false);
        check(c.phase==Phase::Fault && !c.linkReady);check(!c.enable(true));
    }
    {
        Evidence e;e.attached=e.baselineReady=e.recovered=true;check(e.noObservedDisconnect());
        const char *s="<3>CTRL-EVENT-DISCONNECTED bssid=redacted reason=3";
        e.event(parseEvent(s,strlen(s)));check(e.disconnects==1 && !e.noObservedDisconnect());
        const char *near="CTRL-EVENT-CONNECTED-extra";check(parseEvent(near,strlen(near))==LinkEvent::Other);
        const char *end="<3>CTRL-EVENT-TERMINATING";e.event(parseEvent(end,strlen(end)));check(e.gap && !e.attached);
        check(parseEvent(nullptr,0)==LinkEvent::Other);
    }
    printf("checks=%u hardware-requests=0\n",checks);
}
