#include "shutter_animation_core.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <QRegularExpression>
#include <cmath>
#include <cstdio>
#include <cstdlib>
static int checks;
static void check(bool yes,const char*why){++checks;if(!yes){std::fprintf(stderr,"ANIMATION_FAIL %s %d\n",why,checks);std::exit(1);}}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);check(argc==2,"oracle-path");QFile file(QString::fromLocal8Bit(argv[1]));check(file.open(QIODevice::ReadOnly),"oracle-read");auto qml=QString::fromUtf8(file.readAll());QJSEngine js;
 QString script="function clamp(x){return Math.max(0,Math.min(1,x))};function smooth(x){x=clamp(x);return x*x*(3-2*x)};";
 for(const auto&name:{"closure","openRadius","apertureRadius","flight"}){
  auto match=QRegularExpression("readonly property real "+QString(name)+": ([\\s\\S]*?)(?=\\n    (?:readonly|function))").match(qml);check(match.hasMatch(),"formula-from-fixed-QML");script+="var "+QString(name)+"="+match.captured(1)+"\n;\n";
 }
 // 原始Canvas路径调用被记录成数值；不运行旧页面/网络。
 auto begin=qml.indexOf("for (var i=0;i<8;i++)");auto end=qml.indexOf("if (!root.warmed)",begin);check(begin>0&&end>begin,"original-leaf-loop");
 QString leaf=qml.mid(begin,end-begin);
 QString context="var points=[];var ctx={beginPath:function(){},closePath:function(){},fill:function(){},stroke:function(){},moveTo:function(x,y){points.push(x,y)},lineTo:function(x,y){points.push(x,y)},quadraticCurveTo:function(a,b,c,d){points.push(a,b,c,d)}};";
 for(int i=0;i<=800;i++){
  double progress=double(i)/800;js.globalObject().setProperty("progress",progress);
  auto result=js.evaluate(script+"var root={progress:progress,closure:closure,apertureRadius:apertureRadius,flight:flight,smooth:smooth};var radius=root.apertureRadius,twist=.60*root.closure;"+context+leaf+"[closure,apertureRadius,flight,points]");if(result.isError())std::fprintf(stderr,"%s\n",result.toString().toUtf8().constData());check(!result.isError(),"oracle-evaluated");
  auto f=x2dui::shutter_animation_frame(progress);
  auto near=[](double a,double b){return std::abs(a-b)<1e-8;};
  check(near(f.closure,result.property(0).toNumber()),"closure");check(near(f.radius,result.property(1).toNumber()),"radius");check(near(f.flight,result.property(2).toNumber()),"flight");
  auto points=result.property(3);check(points.property("length").toInt()==8*14,"path-point-count");
  for(int n=0;n<8;n++){
   const auto&v=f.leaves[n];x2dui::AnimationPoint expected[]={v.a,v.b,v.controlB,v.outerB,v.outerA,v.controlA,v.a};
   for(int j=0;j<7;j++){check(near(expected[j].x,points.property(n*14+j*2).toNumber()),"leaf-x");check(near(expected[j].y,points.property(n*14+j*2+1).toNumber()),"leaf-y");}
  }
  for(const auto&field:{"x","y","scale","rotation","opacity"}){
   auto block=qml.mid(qml.lastIndexOf("text: \"☆\""));auto m=QRegularExpression("\\n            "+QString(field)+": ([^\\n]+)").match(block);check(m.hasMatch(),"star-expression");
   js.evaluate("var width=0,height=0");auto v=js.evaluate(m.captured(1));check(!v.isError(),"star-oracle");
   double actual=QString(field)=="x"?f.starX:QString(field)=="y"?f.starY:QString(field)=="scale"?f.starScale:QString(field)=="rotation"?f.starRotation:f.starOpacity;
   check(near(actual,v.toNumber()),"star-differential");
  }
 }
 x2dui::ShutterAnimationCore core;
 check(!core.update(true,false,true,2,true,true,0)&&!core.visible(),"idle-no-play");
 check(core.update(true,true,true,2,true,true,20)&&core.visible()&&core.playCount()==1,"one-exposure-one-sound");
 check(!core.update(true,true,true,2,true,true,50)&&core.playCount()==1,"same-state-no-repeat");
 check(core.frame(820).progress==1&&core.visible(),"800ms-end-still-overlay");
 check(core.frame(2020).overlayOpacity==0&&!core.visible(),"2s-watchdog");
 check(!core.update(true,true,true,2,true,true,2100)&&!core.visible(),"watchdog-no-restart");
 core.update(true,false,true,2,true,true,2200);check(!core.update(true,true,true,1,true,true,2250)&&core.visible(),"animation-only-no-sound");
 core.update(false,false,true,1,true,true,2300);check(!core.visible(),"source-loss");
 check(!core.update(true,true,false,2,true,true,2400)&&!core.visible(),"unready-no-begin");
 check(!core.update(true,true,true,2,true,true,2500)&&!core.visible(),"ready-during-exposure-no-replay");
 core.update(true,false,true,2,true,false,2600);check(!core.update(true,true,true,2,true,false,2650)&&core.visible(),"network-off-no-audio");
 core.update(true,true,true,0,true,true,2700);check(!core.visible(),"disabled-hides");
 core.update(true,false,true,2,true,true,2800);core.update(true,true,true,2,true,true,2810);core.frame(2809);check(!core.visible(),"clock-regression-hides");
 std::printf("SHUTTER_ANIMATION_PASS assertions=%d frames=801 camera_requests=0\n",checks);
}
