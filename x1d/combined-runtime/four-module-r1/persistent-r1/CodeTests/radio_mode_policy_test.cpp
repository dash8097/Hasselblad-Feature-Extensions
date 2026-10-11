#include "../native/radio_mode_policy.h"
#include <cassert>
int main() {
    using P=RadioModePolicy;
    for(unsigned from=0;from<3;++from) for(unsigned to=0;to<3;++to) {
        P p;p.current=P::Mode(from);p.verified=true;
        auto action=p.request(to,100,false);
        if(from==to){assert(action==P::None);continue;}
        assert(action==P::DisableFlash && !p.verified);
        auto ticket=p.transaction;
        assert(p.request((to+1)%3,101,false)==P::None);
        assert(p.stopped(ticket,102,false,true,false)==P::None);
        assert(p.stopped(ticket,103,false,false,true)==P::None);
        assert(p.stopped(ticket-1,104,false,false,false)==P::None);
        auto expected=to==P::Wifi?P::SetWifi:to==P::Flash?P::SetFlash:P::SetOff;
        assert(p.stopped(ticket,105,false,false,false)==expected);
        p.hardwareDone(ticket-1,106,true);assert(p.phase==P::SwitchHardware);
        auto next=p.hardwareDone(ticket,107,true);
        if(to==P::Flash){assert(next==P::EnableFlash && !p.verified);p.flashReady(ticket,false);assert(!p.verified);p.flashReady(ticket,true);}
        assert(p.verified && p.current==P::Mode(to) && !p.busy());
    }
    P p;assert(p.request(P::Wifi,0,true)==P::None);
    assert(p.request(3,0,false)==P::None);
    p.request(P::Wifi,0,false);assert(p.tick(4999)==P::None);
    assert(p.tick(5000)==P::DisableFlash && !p.verified);
    p.stopped(p.transaction,5001,false,false,false);assert(p.phase==P::Failed);
    assert(p.request(P::Off,5002,false)==P::DisableFlash);
    p.stopped(p.transaction,5003,false,false,false);p.hardwareDone(p.transaction,5004,false);
    assert(p.phase==P::Failed && !p.verified);
}
