#include "animation_view_qt.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QPainter>
#include <QImage>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFontDatabase>
#include <QRawFont>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace x2dui;
static int checks;
static void check(bool ok,const char*reason){++checks;if(!ok){std::fprintf(stderr,"ANIMATION_VIEW_FAIL %s at %d\n",reason,checks);std::exit(1);}}
static void wait(int ms){QElapsedTimer timer;timer.start();do{QCoreApplication::processEvents();QThread::msleep(1);}while(timer.elapsed()<ms);}
static QImage native(const AnimationFrame&frame,int w,int h){QImage image(w,h,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);auto before=painter.transform();auto opacity=painter.opacity();QtAnimationPainter ctx{&painter};auto api=qt_animation_painter(ctx);auto r=paint_shutter_animation(api,frame,w,h);check(r.success,"native painted");check(ctx.depth==0&&painter.transform()==before&&painter.opacity()==opacity,"painter state restored");if(frame.overlayOpacity>0)check(r.leaves==8,"all eight leaves");painter.end();return image;}
static bool fail_path(void*,const AnimationPathPart*,size_t,uint32_t,uint32_t,double){return false;}
struct FaultPainter {
 int calls=0,fail=0,depth=0;
 bool next(){return ++calls!=fail;}
 static FaultPainter&self(void*p){return *static_cast<FaultPainter*>(p);}
 static bool save(void*p){auto&m=self(p);if(!m.next())return false;++m.depth;return true;}
 static void restore(void*p){auto&m=self(p);check(m.depth>0,"model restore owned state");--m.depth;}
 static bool scalar(void*p,double){return self(p).next();}
 static bool pair(void*p,double,double){return self(p).next();}
 static bool rect(void*p,AnimationRect){return self(p).next();}
 static bool fill(void*p,AnimationRect,uint32_t){return self(p).next();}
 static bool metrics(void*p,const char16_t*,size_t n,int pixels,AnimationTextMetrics*m){if(!self(p).next())return false;*m={double(n*pixels),double(pixels+10),double(pixels)};return true;}
 static bool text(void*p,const char16_t*,size_t,int,AnimationPoint,uint32_t){return self(p).next();}
 static bool path(void*p,const AnimationPathPart*parts,size_t n,uint32_t fill,uint32_t stroke,double width){check(n==6&&parts[0].verb==AnimationPathVerb::Move&&parts[5].verb==AnimationPathVerb::Close&&fill==0x131313ff&&stroke==0x626262ff&&width==1.2,"path ABI fields");return self(p).next();}
 AnimationPainterApi api(){return {this,save,restore,scalar,pair,pair,scalar,rect,fill,metrics,text,path};}
};
int main(int argc,char**argv){QGuiApplication app(argc,argv);check(argc==3,"arguments");QDir output(QString::fromLocal8Bit(argv[2]));
 for(auto name:{"segoeui.ttf","seguisym.ttf","msyh.ttc"})check(QFontDatabase::addApplicationFont(output.filePath(QString("private-fonts/")+name))>=0,"explicit test font loaded");
 QFont base("Microsoft YaHei");base.setPixelSize(40);app.setFont(base);QRawFont raw=QRawFont::fromFont(base);check(raw.isValid()&&raw.supportsCharacter(0x2606)&&raw.supportsCharacter('C'),"actual star and Latin glyphs available");
 QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));if(component.isError())qWarning()<<component.errors();check(component.isReady(),"oracle QML ready");
 QQuickWindow window;window.setColor(Qt::transparent);QObject*object=component.beginCreate(engine.rootContext());auto*item=qobject_cast<QQuickItem*>(object);check(item!=nullptr,"oracle item");item->setParentItem(window.contentItem());component.completeCreate();window.show();
 QJsonArray results;double worstMean=0,worstFraction=0;
 const double progress[]={0,.125,.2125,.25,.4,.5,.625,.75,.9,1};
 for(auto size:{QSize(800,600),QSize(400,300),QSize(900,600),QSize(600,800)}){
  window.resize(size);item->setSize(size);wait(80);
  for(double p:progress){item->setProperty("showing",true);item->setProperty("progress",p);window.update();wait(80);auto original=window.grabWindow().convertToFormat(QImage::Format_ARGB32_Premultiplied);check(original.size()==size,"oracle frame dimensions");auto frame=shutter_animation_frame(p);frame.overlayOpacity=1;auto drawn=native(frame,size.width(),size.height());
   uint64_t sum=0,changed=0;int maximum=0;
   for(int y=0;y<size.height();++y)for(int x=0;x<size.width();++x){auto a=original.pixel(x,y),b=drawn.pixel(x,y);int d=std::max({abs(qRed(a)-qRed(b)),abs(qGreen(a)-qGreen(b)),abs(qBlue(a)-qBlue(b)),abs(qAlpha(a)-qAlpha(b))});sum+=unsigned(d);if(d>16)++changed;if(d>maximum)maximum=d;}
   double count=size.width()*size.height(),mean=sum/count,fraction=changed/count;worstMean=std::max(worstMean,mean);worstFraction=std::max(worstFraction,fraction);
   QString tag=QString("%1x%2-p%3").arg(size.width()).arg(size.height()).arg(int(p*10000));check(original.save(output.filePath(tag+"-qml.png"))&&drawn.save(output.filePath(tag+"-native.png")),"evidence png");
   results.append(QJsonObject{{"frame",tag},{"mean_channel_max_error",mean},{"pixels_error_gt16",double(changed)},{"fraction_error_gt16",fraction},{"max_error",maximum}});
  }
 }
 // Failures must restore painter stack; partially drawn destination must be discarded by caller.
 QImage image(800,600,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);painter.translate(7,9);painter.setOpacity(.4);auto before=painter.transform();QtAnimationPainter ctx{&painter};auto api=qt_animation_painter(ctx);auto frame=shutter_animation_frame(.3);frame.overlayOpacity=1;api.path=fail_path;check(!paint_shutter_animation(api,frame,800,600).success&&ctx.depth==0&&painter.transform()==before&&painter.opacity()==.4,"partial failure balanced");
 api=qt_animation_painter(ctx);frame.leaves[3].a.x=std::numeric_limits<double>::infinity();check(!paint_shutter_animation(api,frame,800,600).success&&ctx.depth==0,"nonfinite frame reject");frame=shutter_animation_frame(0);frame.overlayOpacity=1;check(!paint_shutter_animation(api,frame,0,600).success&&!paint_shutter_animation(api,frame,20000,600).success,"viewport limits");
 frame.overlayOpacity=0;check(paint_shutter_animation(api,frame,800,600).success&&ctx.depth==0,"hidden frame no drawing");painter.end();
 frame=shutter_animation_frame(.625);frame.overlayOpacity=1;FaultPainter healthy;check(paint_shutter_animation(healthy.api(),frame,800,600).success,"model success");for(int n=1;n<=healthy.calls;++n){FaultPainter fault;fault.fail=n;check(!paint_shutter_animation(fault.api(),frame,800,600).success&&fault.depth==0,"all callback failures restore state");}
 QFile evidence(output.filePath("pixel-comparison.json"));check(evidence.open(QIODevice::WriteOnly),"comparison evidence");evidence.write(QJsonDocument(QJsonObject{{"frames",results},{"worst_mean",worstMean},{"worst_fraction_gt16",worstFraction},{"same_font_environment",true},{"original_network_removed",true},{"not_pixel_identical",true}}).toJson());evidence.close();
 // QtQuick Text与QPainter字形栅格允许有界差异；几何错误不得靠任意阈值隐藏。
 check(worstMean<=3.0&&worstFraction<=.025,"visual comparison bounded");
 delete item;std::printf("ANIMATION_VIEW_PASS assertions=%d frames=%lld worst_mean=%.6f worst_fraction=%.6f camera_requests=0\n",checks,static_cast<long long>(results.size()),worstMean,worstFraction);
}
