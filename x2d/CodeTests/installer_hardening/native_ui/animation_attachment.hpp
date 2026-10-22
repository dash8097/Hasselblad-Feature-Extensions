#pragma once
#include "native_animation_view.hpp"
#include "tree_observer.hpp"
#include "effect_service.h"
#include "sound_service.h"
namespace x2dui {
// Clock uses the same monotonic milliseconds as the GUI scheduler. Injectable
// only at native construction; never accepts commands or executable text.
struct AnimationClock {void*context;uint64_t(*now)(void*);};
class AnimationAttachment {
 ViewApi api;StockProfile stock;MenuProfile menu;SignalProfile signals;AnimationImageProfile image;
 X2dEffectService*effect;X2dSoundService sound{};AnimationClock clock;TreeObserver tree;
 ShutterAnimationCore core;NativeAnimationView*view=nullptr;void*root=nullptr,*drawer=nullptr;
 SignalWatch*rootDeath=nullptr,*drawerDeath=nullptr,*stateChange=nullptr;
 bool ticking=false,closing=false,closePending=false,lost=false,stateDirty=true;uint64_t generation=0;
 static void rootDestroyed(void*);static void sourceDestroyed(void*);static void stateChanged(void*);static void released(void*){}
 bool discover(void*,void*&,unsigned&,unsigned&,unsigned);
 bool observe(uint64_t);bool bindSource(void*);void finishTick();
 struct TickScope {AnimationAttachment&s;explicit TickScope(AnimationAttachment&v):s(v){s.ticking=true;}~TickScope(){s.finishTick();}};
public:
 AnimationAttachment(ViewApi a,StockProfile s,MenuProfile m,SignalProfile g,AnimationImageProfile i,X2dEffectService*e,AnimationClock c):api(a),stock(s),menu(m),signals(g),image(i),effect(e),clock(c),tree(m,a,g){x2d_sound_init(&sound);}
 ~AnimationAttachment(){close();}
 bool tick(void*freshRoot,uint64_t now,double width,double height);
 void close();bool active()const{return core.visible();}
 bool has_source()const{return drawer&&!lost;}
};
}
