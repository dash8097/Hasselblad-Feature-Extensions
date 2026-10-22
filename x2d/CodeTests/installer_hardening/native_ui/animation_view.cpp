#include "animation_view.hpp"
#include <cfloat>
#include <initializer_list>
namespace x2dui {
namespace {
bool finite(double v){return v==v&&v<=DBL_MAX&&v>=-DBL_MAX;}
bool coordinate(double v){return finite(v)&&v>=-100000&&v<=100000;}
bool metric(const AnimationTextMetrics&m){return finite(m.width)&&finite(m.height)&&finite(m.ascent)&&m.width>=0&&m.width<=4000&&m.height>0&&m.height<=400&&m.ascent>=0&&m.ascent<=m.height;}
double ceil_nonnegative(double v){auto i=static_cast<unsigned>(v);return v>i?double(i+1):double(i);}
struct Saved {
 const AnimationPainterApi&a;bool held;
 explicit Saved(const AnimationPainterApi&api):a(api),held(api.save(api.ctx)){}
 ~Saved(){if(held)a.restore(a.ctx);}
};
constexpr char16_t greeting[]=u"Ciallo～(∠・ω< )⌒";
constexpr char16_t star[]=u"☆";
}
AnimationPaintResult paint_shutter_animation(const AnimationPainterApi&a,const AnimationFrame&f,double width,double height){
 AnimationPaintResult result;
 if(!a.ctx||!a.save||!a.restore||!a.opacity||!a.translate||!a.scale||!a.rotate||!a.clip||!a.fill_rect||!a.text_metrics||!a.text||!a.path)return result;
 if(!finite(width)||!finite(height)||width<=0||height<=0||width>16384||height>16384||!finite(f.overlayOpacity)||f.overlayOpacity<0||f.overlayOpacity>1)return result;
 if(!coordinate(f.starX)||!coordinate(f.starY)||!coordinate(f.starRotation)||!finite(f.starScale)||f.starScale<0||f.starScale>10||!finite(f.starOpacity)||f.starOpacity<0||f.starOpacity>1)return result;
 for(const auto&leaf:f.leaves)for(auto p:{leaf.a,leaf.b,leaf.controlB,leaf.outerB,leaf.outerA,leaf.controlA})if(!coordinate(p.x)||!coordinate(p.y))return result;
 if(f.overlayOpacity==0){result.success=true;return result;}
 Saved frame(a);if(!frame.held)return result;
 if(!a.opacity(a.ctx,f.overlayOpacity)||!a.fill_rect(a.ctx,{0,0,width,height},0x000000ff))return result;
 double sx=width/800,sy=height/600,scale=sx<sy?sx:sy;
 if(!a.translate(a.ctx,(width-800*scale)/2,(height-600*scale)/2)||!a.scale(a.ctx,scale,scale))return result;
 AnimationTextMetrics message,smallStar,bigStar;
 if(!a.text_metrics(a.ctx,greeting,sizeof(greeting)/sizeof(char16_t)-1,40,&message)||!metric(message)||
    !a.text_metrics(a.ctx,star,1,40,&smallStar)||!metric(smallStar)||!a.text_metrics(a.ctx,star,1,100,&bigStar)||!metric(bigStar))return result;
 // QQuickText implicitWidth/Height 使用向上取整的布局尺寸，opacity=0的星仍占Row空间。
 auto messageWidth=ceil_nonnegative(message.width),starWidth=ceil_nonnegative(smallStar.width);
 auto rowHeight=ceil_nonnegative(message.height>smallStar.height?message.height:smallStar.height);
 double left=400-(messageWidth+starWidth)/2,top=300-rowHeight/2;
 if(!a.text(a.ctx,greeting,sizeof(greeting)/sizeof(char16_t)-1,40,{left,top+message.ascent},0xffffffff))return result;++result.texts;
 if(f.inlineStar){if(!a.text(a.ctx,star,1,40,{left+messageWidth,top+smallStar.ascent},0xffffffff))return result;++result.texts;}
 {
  Saved canvas(a);if(!canvas.held||!a.clip(a.ctx,{0,0,800,600}))return result;
  for(const auto&leaf:f.leaves){AnimationPathPart p[]={
    {AnimationPathVerb::Move,leaf.a,{}},{AnimationPathVerb::Line,leaf.b,{}},
    {AnimationPathVerb::Quadratic,leaf.outerB,leaf.controlB},{AnimationPathVerb::Line,leaf.outerA,{}},
    {AnimationPathVerb::Quadratic,leaf.a,leaf.controlA},{AnimationPathVerb::Close,{},{}}};
   if(!a.path(a.ctx,p,6,0x131313ff,0x626262ff,1.2))return result;++result.leaves;
  }
 }
 if(f.starOpacity>0&&f.starScale>0){
  Saved flying(a);if(!flying.held||!a.translate(a.ctx,f.starX,f.starY)||!a.rotate(a.ctx,f.starRotation)||!a.scale(a.ctx,f.starScale,f.starScale)||!a.opacity(a.ctx,f.overlayOpacity*f.starOpacity))return result;
  if(!a.text(a.ctx,star,1,100,{-ceil_nonnegative(bigStar.width)/2,bigStar.ascent-ceil_nonnegative(bigStar.height)/2},0xffffffff))return result;++result.texts;
 }
 result.success=true;return result;
}
}
