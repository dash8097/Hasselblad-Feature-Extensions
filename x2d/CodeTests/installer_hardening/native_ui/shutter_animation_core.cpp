#include "shutter_animation_core.hpp"
#include <cmath>
#include <limits>
namespace x2dui {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
double clamp(double x){return !std::isfinite(x)||x<0?0:x>1?1:x;}
double smooth(double x){x=clamp(x);return x*x*(3-2*x);}
AnimationPoint polar(double r,double angle){return {400+r*std::cos(angle),300+r*std::sin(angle)};}
}
AnimationFrame shutter_animation_frame(double progress){
 AnimationFrame f;f.progress=clamp(progress);
 f.closure=clamp(f.progress/.25)*(1-clamp((f.progress-.5)/.25));
 f.radius=f.progress<.25?250*(1-clamp(f.progress/.25)):f.progress<.5?0:(400+300*std::tan(pi/8))*clamp((f.progress-.5)/.25);
 f.flight=clamp((f.progress-.2125)/.7875);double t=f.flight;
 f.starX=400+300*t;f.starY=300-800*t*(1-t)+60*t;
 f.starScale=smooth(t/.25)*(1+.20*std::sin(t*pi));f.starRotation=-20+40*t;
 f.starOpacity=smooth(t/.1)*(1-smooth((f.progress-.92)/.08));f.inlineStar=f.progress<.2125||f.progress>=1;
 for(int i=0;i<8;i++){double a=i*pi/4+.60*f.closure,b=a+pi/4;f.leaves[i]={polar(f.radius,a),polar(f.radius,b),polar(720,b+.25),polar(1500,b+.8),polar(1500,a+.8),polar(720,a+.25)};}
 return f;
}
bool ShutterAnimationCore::update(bool hasSource,bool isExposing,bool preferencesReady,int effectMode,bool soundReady,bool networkEnabled,std::uint64_t now){
 if(!initialized){initialized=true;created=now;last=now;}
 if(now<last){showing=false;source=hasSource;exposing=hasSource&&isExposing;last=now;created=now;return false;}
 last=now;bool next=hasSource&&isExposing,edge=next&&!exposing;
 source=hasSource;exposing=next;ready=preferencesReady;mode=effectMode;audioReady=soundReady;network=networkEnabled;
 if(!source||!exposing||!ready||mode<1||mode>2){showing=false;return false;}
 if(showing&&now-started>=2000)showing=false;
 if(!edge)return false;
 started=now;showing=true;if(plays!=std::numeric_limits<std::uint64_t>::max())++plays;
 return audioReady&&mode==2&&network;
}
AnimationFrame ShutterAnimationCore::frame(std::uint64_t now){
 if(!initialized)return shutter_animation_frame(0);
 if(now<last){showing=false;created=now;}last=now;
 if(showing&&now-started>=2000)showing=false;
 auto f=shutter_animation_frame(showing?static_cast<double>(now-started)/800:0);
 f.overlayOpacity=showing?1:now-created<80?.001:0;return f;
}
}
