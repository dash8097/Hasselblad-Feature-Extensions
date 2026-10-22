#include "screen_controls.hpp"
#include "screen_mode_session.hpp"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned checks;
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"SCREEN_CONTROL_FAIL line=%d\n",__LINE__);exit(1);}}while(0)
struct Proxy {std::vector<ScreenWrite>writes;bool ok=true;static bool apply(void*p,const ScreenWrite&w){auto&s=*static_cast<Proxy*>(p);s.writes.push_back(w);return s.ok;}};
struct ModeProxy {unsigned live=0;std::vector<int>writes;std::vector<void*>attached,removed;bool ok=true;static bool liveview(void*p){auto&s=*static_cast<ModeProxy*>(p);++s.live;return s.ok;}static bool forward(void*p,int v){auto&s=*static_cast<ModeProxy*>(p);s.writes.push_back(v);return s.ok;}static bool keys(void*p,void*o,bool a){auto&s=*static_cast<ModeProxy*>(p);(a?s.attached:s.removed).push_back(o);return s.ok;}ScreenModeApi api(){return{this,liveview,forward,keys};}};
struct ReenterControl {ScreenControl*c=nullptr;ScreenControlSnapshot s;unsigned calls=0;static bool apply(void*p,const ScreenWrite&){auto&r=*static_cast<ReenterControl*>(p);++r.calls;C(r.c->dispatching());C(!r.c->update(r.s));C(!r.c->choose(1,1));r.c->tick(2);r.c->close();return true;}};
struct ReenterMode {ScreenModeSession*c=nullptr;int point=0;unsigned live=0,attach=0,remove=0,write=0;static bool l(void*p){auto&r=*static_cast<ReenterMode*>(p);++r.live;if(r.point==1)r.c->close();return true;}static bool f(void*p,int){auto&r=*static_cast<ReenterMode*>(p);++r.write;if(r.point==3)r.c->close();return true;}static bool k(void*p,void*,bool a){auto&r=*static_cast<ReenterMode*>(p);if(a){++r.attach;if(r.point==2)r.c->close();}else ++r.remove;return true;}};
int main(){const char16_t*display[]={u"自动",u"屏幕",u"取景器"};for(int initial=0;initial<3;initial++)for(int selection=0;selection<4;selection++)for(int evf=0;evf<2;evf++){
 Proxy p;ScreenControl c(ScreenControlKind::Display,{&p,Proxy::apply});ScreenControlSnapshot s;s.enabled=true;s.labels=display;s.count=3;s.current=display[initial];s.evfOnly=evf;s.backBit=2;s.evfBit=8;s.screensAllowed=2;C(c.update(s)&&c.count()==4&&c.available());C(c.choose(selection,100)&&c.busy());C(p.writes.size()==1&&p.writes[0].kind==ScreenWriteKind::EvfOnly&&p.writes[0].value==(selection==3));C(!c.choose(0,100));s.evfOnly=selection==3;C(c.update(s));c.tick(101);C(p.writes.size()==2);C(p.writes[1].kind==(selection==3?ScreenWriteKind::ScreensAllowed:ScreenWriteKind::DisplayStock));if(selection==3){C(p.writes[1].value==10);s.screensAllowed=10;}else{C(p.writes[1].sourceIndex==selection);s.current=display[selection];}C(c.update(s));c.tick(102);C(c.take_complete()&&!c.busy()&&!c.error());c.tick(9999);C(p.writes.size()==2);
 }
 const char16_t*timeouts[]={u"关闭",u"30秒",u"1分钟",u"禁用",u"5分钟"};for(int wanted=0;wanted<4;wanted++){Proxy p;ScreenControl c(ScreenControlKind::Timeout,{&p,Proxy::apply});ScreenControlSnapshot s;s.enabled=true;s.labels=timeouts;s.count=5;s.zero=u"禁用";s.current=u"1分钟";C(c.update(s)&&c.count()==4&&c.current()==1);C(c.choose(wanted,0));C(p.writes.size()==1);if(wanted==3){C(p.writes[0].kind==ScreenWriteKind::TimeoutZero);s.timeoutZero=true;}else{s.current=c.label(wanted);C(p.writes[0].kind==ScreenWriteKind::TimeoutStock&&p.writes[0].sourceIndex==(wanted==0?1:wanted==1?2:4));}C(c.update(s));c.tick(1);C(c.take_complete());}
 for(int value=0;value<2;value++){Proxy p;ScreenControl c(ScreenControlKind::AdaptiveFps,{&p,Proxy::apply});ScreenControlSnapshot s;s.enabled=true;s.fps=value;C(c.update(s)&&c.current()==value);C(c.choose(!value,0));C(p.writes.size()==1&&p.writes[0].kind==ScreenWriteKind::AdaptiveFps&&p.writes[0].value==!value);s.fps=!value;C(c.update(s));c.tick(1);C(c.take_complete());}
 Proxy p;ScreenControl c(ScreenControlKind::Display,{&p,Proxy::apply});ScreenControlSnapshot s;s.enabled=true;s.labels=display;s.count=3;s.current=display[0];s.backBit=2;s.evfBit=8;C(c.update(s));p.ok=false;C(!c.choose(3,0)&&c.error());for(int i=0;i<100;i++)c.tick(i*100);C(p.writes.size()==1);p.ok=true;C(c.choose(3,10000));c.tick(15000);C(c.error()&&!c.busy()&&p.writes.size()==2);
 C(c.choose(0,16000));s.evfOnly=false;const char16_t*reordered[]={u"屏幕",u"自动",u"取景器"};s.labels=reordered;C(c.update(s));c.tick(16001);C(c.error()&&p.writes.size()==3);
 s.labels=display;s.count=4;C(!c.update(s)&&!c.available());s.count=3;char16_t oversized[129];for(int i=0;i<128;i++)oversized[i]=u'x';oversized[128]=0;s.current=oversized;C(!c.update(s));s.current=display[0];s.backBit=3;C(!c.update(s));s.backBit=2;C(c.update(s));C(c.choose(3,0));c.close();c.tick(100);C(c.error()&&!c.available());
 ModeProxy m;ScreenModeSession mode(m.api());ScreenModeSnapshot state;state.mode.evfOnly=state.mode.backAllowed=state.mode.evfAllowed=state.mode.backActive=true;state.focusOwner=reinterpret_cast<void*>(1);state.halfOverrideBit=8;state.ancestorOverride=2;mode.update(state);C(m.attached.size()==1&&m.writes.size()==1&&m.writes[0]==10);C(!mode.press(true,false,0)&&mode.pending());for(int i=0;i<100;i++)mode.update(state);C(m.writes.size()==1);state.forward=10;mode.update(state);C(mode.intercepting()&&!mode.pending());C(mode.press(true,false,100)&&mode.notice(1099)&&!mode.notice(1100));C(mode.release(true));state.focusOwner=reinterpret_cast<void*>(2);mode.update(state);C(m.attached.size()==2&&m.removed.size()==1&&m.writes.size()==1);state.mode.backActive=false;state.mode.evfActive=state.mode.modelPresent=state.mode.modelEvf=true;mode.update(state);C(m.live==1&&m.removed.size()==2&&m.writes.back()==2&&!mode.intercepting());for(int i=0;i<100;i++)mode.update(state);C(m.live==1);mode.close();mode.close();C(m.writes.size()==2);
 ModeProxy bad;bad.ok=false;ScreenModeSession rejected(bad.api());state.mode.evfActive=false;state.mode.backActive=true;state.forward=10;rejected.update(state);C(!rejected.intercepting()&&!rejected.press(true,false,1)&&rejected.failures()==1);rejected.close();C(rejected.failures()==3&&bad.removed.size()==1);
 ReenterControl re;ScreenControl reControl(ScreenControlKind::AdaptiveFps,{&re,ReenterControl::apply});re.c=&reControl;re.s.enabled=true;C(reControl.update(re.s));C(reControl.choose(1,0));C(re.calls==1&&!reControl.dispatching()&&!reControl.busy()&&reControl.error());
 for(int point=2;point<=3;point++){ReenterMode r;r.point=point;ScreenModeSession m({&r,ReenterMode::l,ReenterMode::f,ReenterMode::k});r.c=&m;ScreenModeSnapshot x;x.mode.evfOnly=x.mode.backAllowed=x.mode.evfAllowed=x.mode.backActive=true;x.focusOwner=reinterpret_cast<void*>(1);x.halfOverrideBit=8;x.ancestorOverride=2;m.update(x);C(r.attach==1&&r.remove==1&&!m.intercepting()&&!m.dispatching());C(r.write==unsigned(point==3?2:0));m.update(x);m.close();C(r.attach==1&&r.remove==1);}
 for(int seconds:{0,10,20,30,60,120,300}){
  LiveviewTimeoutPolicy timer;C(!timer.update(100,seconds,true,false,false));
  C(!timer.update(100+seconds*1000-unsigned(seconds>0),seconds,true,false,false));
  C(timer.update(100+seconds*1000,seconds,true,false,false)==(seconds>0));
  C(!timer.update(1000000,seconds,true,false,false));
  C(!timer.update(1000001,seconds,true,false,true));
  C(!timer.update(2000000,seconds,false,false,false));
  C(!timer.update(3000000,seconds,true,true,false));
  C(!timer.update(4000000,seconds,true,false,false));
  C(timer.update(4000000+seconds*1000,seconds,true,false,false)==(seconds>0));
  C(!timer.update(1,seconds,true,false,false));
 }
 {LiveviewTimeoutPolicy timer;C(!timer.update(0,10,true,false,false));C(!timer.update(9999,20,true,false,false));C(!timer.update(29998,20,true,false,false));C(timer.update(29999,20,true,false,false));C(!timer.update(50000,-1,true,false,false));C(!timer.waiting());}
 const char16_t*lvLabels[]={u"关闭",u"10秒",u"20秒",u"30秒",u"1分钟",u"2分钟",u"5分钟"};
 for(int value:{0,10,20,30,60,120,300}){Proxy lp;ScreenControl lc(ScreenControlKind::LiveviewTimeout,{&lp,Proxy::apply});ScreenControlSnapshot ls;ls.enabled=true;ls.labels=lvLabels;ls.count=7;ls.liveviewSeconds=value;ls.liveviewConfirmed=true;C(lc.update(ls));int ix=lc.current();C(lc.choose(ix,100));C(lp.writes.size()==1&&lp.writes[0].value==value);ls.liveviewConfirmed=false;C(lc.update(ls));lc.tick(101);C(lc.busy()&&!lc.take_complete());ls.liveviewConfirmed=true;C(lc.update(ls));lc.tick(102);C(lc.take_complete()&&!lc.busy());}
 printf("SCREEN_CONTROLS_PASS assertions=%u\n",checks);
}
