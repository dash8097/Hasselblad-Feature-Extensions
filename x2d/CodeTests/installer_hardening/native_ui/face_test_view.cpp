#include "face_view_qt.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QPainter>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned checks;
static void check(bool v,const char*m){++checks;if(!v){fprintf(stderr,"FACE_VIEW_FAIL %s check=%u\n",m,checks);exit(1);}}
static void wait(){QElapsedTimer t;t.start();do{QCoreApplication::processEvents();QThread::msleep(1);}while(t.elapsed()<60);}
static QRectF rect(FaceRect r){return QRectF(r.x,r.y,r.width,r.height);}
static int roi(FaceRoi r){return r==FaceRoi::Face?1:r==FaceRoi::EyeLeft?2:r==FaceRoi::EyeRight?3:0;}
static void seteye(QObject*o,const EyeModel&e){check(o,"eye fixture exists");auto&i=e.input();o->setProperty("visible",e.visible_eye());o->setProperty("face",QVariantMap{{"faceId",i.faceId},{"leftEye",rect(i.left)},{"rightEye",rect(i.right)}});o->setProperty("focusConfirmed",i.focusConfirmed);o->setProperty("afRoiType",roi(i.roi));o->setProperty("personLocked",i.personLocked);o->setProperty("activeEye",e.state().active);o->setProperty("preferredEye",e.state().preferred);}
struct Fault{unsigned calls=0,fail=0;int depth=0;static bool begin(void*p,double,double){auto&s=*static_cast<Fault*>(p);if(++s.calls==s.fail)return false;++s.depth;return true;}static bool draw(void*p,const FaceDraw&){auto&s=*static_cast<Fault*>(p);return ++s.calls!=s.fail;}static void end(void*p){auto&s=*static_cast<Fault*>(p);--s.depth;}FacePainterApi api(){return {this,begin,draw,end};}};
int main(int argc,char**argv){QGuiApplication app(argc,argv);check(argc==2,"output argument");QDir out(QString::fromLocal8Bit(argv[1]));check(QFontDatabase::addApplicationFont(out.filePath("font.ttc"))>=0,"explicit font");QFont font("Microsoft YaHei");font.setPixelSize(20);app.setFont(font);
 QQmlEngine engine;QQmlComponent c(&engine,QUrl::fromLocalFile(out.filePath("oracle.qml")));if(c.isError())qWarning()<<c.errors();check(c.isReady(),"original visual fragment QML");QQuickWindow window;window.setColor(Qt::black);auto*item=qobject_cast<QQuickItem*>(c.create());check(item,"oracle item");item->setParentItem(window.contentItem());window.show();QJsonArray results;double worstMean=0,worstFraction=0;
 for(auto size:{QSize(800,600),QSize(400,300)})for(int scene=0;scene<12;scene++){
  FaceActions actions;FaceSnapshot s;s.present=s.faceMode=s.idle=s.screenBack=s.canMoveTp=s.canShowAF=true;s.width=size.width();s.height=size.height();s.focusX=.22;s.focusY=.4;s.focusRegion={.45,.45,.08,.08,true};s.count=2;s.candidates[0]={9,{1000,2000,2500,4500,true}};s.candidates[1]={3,{6500,2000,2200,4000,true}};s.detectedId=9;s.detectedValid=true;s.detectedRegion={.1,.2,.25,.45,true};s.roi=FaceRoi::Face;s.foundFocus=scene==3;
  if(scene==0){s.detectedValid=false;s.detectedId=-1;}if(scene==7)s.paused=true;if(scene==8)s.canShowAF=false;if(scene==9)s.faceMode=false;if(scene==10){s.detectedRegion={-.08,-.02,.25,.3,true};s.candidates[0].region={-800,-200,2500,3000,true};}
  FacePointModel p;check(p.update(s,0,actions),"snapshot accepted");FaceAction a;while(actions.take(a)){}if(scene>=2&&scene!=9){check(p.lock_nearest(actions),"lock near face");while(actions.take(a)){}}if(scene==11){s.count=0;s.detectedValid=false;check(p.update(s,100,actions),"lost face snapshot");p.tick(1100,actions);}
  EyeInput ei;ei.width=s.width;ei.height=s.height;ei.faceId=s.detectedId;ei.faceValid=p.actual_face_valid();ei.eyeMode=scene>=4&&scene<=6;ei.left={.14,.3,.025,.025,true};ei.right={.24,.3,.025,.025,true};ei.personLocked=p.state().personLocked;ei.lockedFaceId=p.state().selectedId;ei.focusConfirmed=scene==6;ei.roi=FaceRoi::EyeRight;EyeModel eye,fallback;eye.update(ei,0);if(ei.personLocked)eye.select_at(9,.25*s.width,.31*s.height,0);if(scene==5){ei.right.present=false;eye.update(ei,1);eye.tick(501);}EyeInput empty;empty.width=s.width;empty.height=s.height;fallback.update(empty,0);
  FaceFrame f;check(face_frame(p,eye,fallback,f),"frame generated");check(f.count<=560&&f.valid,"bounded complete frame");QImage native(size,QImage::Format_ARGB32_Premultiplied);native.fill(Qt::black);QPainter painter(&native);painter.setFont(font);FaceQtPainter q{&painter};check(face_paint(f,face_qt_painter(q))&&q.depth==0,"native painted/restored");painter.end();
  auto&state=p.state();window.resize(size);item->setSize(size);item->setProperty("faceMode",s.faceMode);item->setProperty("ordinaryAfFallback",state.ordinaryFallback);item->setProperty("personLocked",state.personLocked);item->setProperty("lockedRegion",rect(state.lockedRegion));item->setProperty("selectorOnly",p.selector_only());item->setProperty("pointPending",state.pointPending);item->setProperty("pointX",state.pointX);item->setProperty("pointY",state.pointY);item->setProperty("actualFaceValid",p.actual_face_valid());item->setProperty("selectedId",state.selectedId);item->setProperty("eyeTargetVisible",eye.visible_eye()||fallback.visible_eye());
  QVariantList faces;for(size_t n=0;n<s.count;n++){auto&v=s.candidates[n];faces<<QVariantMap{{"id",v.id},{"x",v.region.x},{"y",v.region.y},{"width",v.region.width},{"height",v.region.height}};}item->setProperty("candidateFaces",faces);item->setProperty("vm",QVariantMap{{"canShowAF",s.canShowAF},{"faceDetectionPaused",s.paused},{"focusRegion",rect(s.focusRegion)},{"detectedFace",QVariantMap{{"faceId",s.detectedId}}},{"afRoiType",roi(s.roi)},{"afIndicatorStatus",s.foundFocus?3:0}});seteye(item->findChild<QObject*>("eyeMain"),eye);seteye(item->findChild<QObject*>("eyeFallback"),fallback);window.update();wait();QImage original=window.grabWindow().convertToFormat(QImage::Format_ARGB32_Premultiplied);check(original.size()==size,"Qt source frame captured");
  uint64_t sum=0,changed=0;for(int y=0;y<size.height();y++)for(int x=0;x<size.width();x++){auto a=original.pixel(x,y),b=native.pixel(x,y);int d=std::max({abs(qRed(a)-qRed(b)),abs(qGreen(a)-qGreen(b)),abs(qBlue(a)-qBlue(b)),abs(qAlpha(a)-qAlpha(b))});sum+=d;if(d>16)++changed;}double mean=double(sum)/(size.width()*size.height()),fraction=double(changed)/(size.width()*size.height());worstMean=std::max(worstMean,mean);worstFraction=std::max(worstFraction,fraction);QString tag=QString("%1x%2-scene%3").arg(size.width()).arg(size.height()).arg(scene);check(original.save(out.filePath(tag+"-qml.png"))&&native.save(out.filePath(tag+"-native.png")),"save images");results.append(QJsonObject{{"scene",tag},{"mean_error",mean},{"fraction_gt16",fraction},{"commands",int(f.count)}});
  for(unsigned failure=1;failure<=f.count+1;failure++){Fault fault;fault.fail=failure;check(!face_paint(f,fault.api())&&fault.depth==0,"paint failure restores state");}
 }
 QFile metrics(out.filePath("metrics.json"));check(metrics.open(QIODevice::WriteOnly),"metrics open");metrics.write(QJsonDocument(results).toJson());metrics.close();check(worstMean<.03&&worstFraction<.0003,"source visual differential bounded");printf("FACE_VIEW_PASS assertions=%u frames=24 worst_mean=%f worst_fraction=%f\n",checks,worstMean,worstFraction);delete item;
}
