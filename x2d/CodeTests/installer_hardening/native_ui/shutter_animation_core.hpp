#pragma once
#include <cstdint>
namespace x2dui {
struct AnimationPoint {double x,y;};
struct AnimationLeaf {AnimationPoint a,b,controlB,outerB,outerA,controlA;};
struct AnimationFrame {
 double progress=0,closure=0,radius=250,flight=0,starX=400,starY=300,starScale=0,starRotation=-20,starOpacity=0,overlayOpacity=0;
 bool inlineStar=true;AnimationLeaf leaves[8]{};
};
AnimationFrame shutter_animation_frame(double progress);
// 状态观察不触发快门/对焦；调用方只接 GUI 已有 mainState 和设置。
class ShutterAnimationCore {
 bool source=false,exposing=false,ready=false,audioReady=false,network=false,showing=false,initialized=false;
 int mode=0;std::uint64_t started=0,created=0,last=0,plays=0;
public:
 bool update(bool hasSource,bool isExposing,bool preferencesReady,int effectMode,bool soundReady,bool networkEnabled,std::uint64_t now);
 AnimationFrame frame(std::uint64_t now);
 void stop(){showing=false;}
 bool visible()const{return showing;}
 std::uint64_t playCount()const{return plays;}
};
}
