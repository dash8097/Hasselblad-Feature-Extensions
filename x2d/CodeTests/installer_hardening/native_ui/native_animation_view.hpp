#pragma once
#include "stock_factory.hpp"
#include "shutter_animation_core.hpp"
#include "signal_bridge.hpp"
namespace x2dui {
struct AnimationImageProfile {
 unsigned size=0;
 void(*construct)(void*,void*)=nullptr;
 void(*complete)(void*)=nullptr;
 void(*set_context)(void*,void*)=nullptr;
};
// Eight leaves are passive SVG geometry; all state and timing stay in C++.
class NativeAnimationView final:private LocalHeap {
 struct Watch:LocalHeap {NativeAnimationView*self;unsigned index;Watch(NativeAnimationView*s,unsigned i):self(s),index(i){}};
 ViewApi api;StockProfile stock;MenuProfile menu;
 void*nodes[7]{};int refs=1;bool alive=true,updating=false,closed=false,loading=false,failed=false,requested=false;
 SignalWatch*statusWatch=nullptr;AnimationFrame latest{},pending{};double latestWidth=800,latestHeight=600,pendingWidth=800,pendingHeight=600;uint64_t revision=0,pendingRevision=0;
 char svg[8192]{};char16_t uri[12000]{};
 explicit NativeAnimationView(ViewApi a,StockProfile s,MenuProfile m):api(a),stock(s),menu(m){}
 ~NativeAnimationView()=default;
 void retain(){++refs;}void drop(){if(--refs==0)delete this;}
 static void destroyed(void*,Event,double);static void released(void*);
 bool watch(unsigned);bool number(unsigned,const char*,double);bool box(unsigned,double,double,double,double);
 bool metric(unsigned,const char*,double&);bool buildSvg(const AnimationFrame&);
 bool apply(const AnimationFrame&,double,double);bool startImage();bool imageStatus();
 static void statusChanged(void*);static void statusReleased(void*);
public:
 static NativeAnimationView*create(ViewApi,StockProfile,MenuProfile,AnimationImageProfile,SignalProfile,void*parent);
 // True means accepted/coalesced. Pixels become visible only after SVG Ready.
 bool update(const AnimationFrame&,double width,double height);
 void hide();void close();void*item()const{return nodes[0];}
};
}
