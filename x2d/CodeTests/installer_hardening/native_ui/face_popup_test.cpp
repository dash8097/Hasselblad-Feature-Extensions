#include "face_popup_view.hpp"
#include "qt_stock_profile.hpp"
#include "qt_abi_profile.hpp"
#include "qt_signal_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlListReference>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QImage>
#include <QQuickImageProvider>
#include <QFile>
#include <QFontDatabase>
#include <QThread>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QtTest/QTest>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned checks,choices;static int selected=-1;static FacePopupView*active;static bool closeOnChoice=false;static int budget=-1,hits=0;
extern "C" void*__real_malloc(size_t);
extern "C" void*__wrap_malloc(size_t n){if(budget>=0&&budget--==0){++hits;return nullptr;}return __real_malloc(n);}
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"FACE_POPUP_FAIL line=%d\n",__LINE__);exit(1);}}while(0)
static bool choose(void*,int n){++choices;selected=n;if(closeOnChoice){auto*p=active;active=nullptr;p->close();}return true;}
static void wait(){QElapsedTimer t;t.start();do{QCoreApplication::processEvents();QThread::msleep(1);}while(t.elapsed()<100);}
class Icons final:public QQuickImageProvider{public:Icons():QQuickImageProvider(QQuickImageProvider::Image){}QImage requestImage(const QString&id,QSize*size,const QSize&requested)override{QFile f(":/icons/"+id+".svg");if(!f.open(QIODevice::ReadOnly))return{};auto i=QImage::fromData(f.readAll(),"SVG");if(requested.width()>0)i=i.scaledToWidth(qRound(i.width()*requested.width()/100.),Qt::SmoothTransformation);*size=i.size();return i;}};
static FacePopupProfile profile(){static_assert(sizeof(QQmlListProperty<QQuickItem>)==sizeof(FacePopupList));return {QMetaType::fromType<QQmlListProperty<QQuickItem>>().iface()};}

int main(int argc,char**argv){QGuiApplication app(argc,argv);C(argc==3);C(QFontDatabase::addApplicationFont(QString::fromLocal8Bit(argv[2])+"/private-font.ttc")>=0);app.setFont(QFont("Microsoft YaHei"));C(QResource::registerResource(QString::fromLocal8Bit(argv[1])));C(QResource::registerResource(QString::fromLocal8Bit(argv[2])+"/extra.rcc"));QQmlEngine engine;engine.addImageProvider("svg",new Icons);QQuickWindow win;win.resize(1280,960);win.setColor(Qt::black);QQmlEngine::setContextForObject(win.contentItem(),engine.rootContext());auto abi=qt_abi_profile();auto stock=qt_stock_profile();auto api=abi_view_api(&abi);auto sig=qt_signal_profile();FacePopupSnapshot s;s.attached=s.canChange=s.multiAllowed=s.offAllowed=true;s.current=1;FacePopupStyle style;
 auto create=[&](){auto*v=FacePopupView::create(api,stock,sig,profile(),win.contentItem(),win.contentItem(),choose,nullptr);C(v);C(v->update(s,style,1280,960,true));return v;};active=create();win.show();C(active->selection()==1&&!active->finished());auto*root=static_cast<QQuickItem*>(active->item());C(!root->property("handleDefaultKeys").toBool());auto*c0=root->findChild<QQuickItem*>("X2dNativeFaceChoice0");C(c0&&c0->parentItem()!=root&&c0->parentItem()->parentItem()!=root);C(c0->parentItem()->width()==848);C(c0->height()>140);C(!c0->property("acceptKeys").toBool());C(active->key(FacePopupKey::Previous)&&active->selection()==0);C(active->key(FacePopupKey::Accept)&&choices==1&&selected==0&&!active->finished());s.busy=true;C(active->update(s,style,1280,960,true));C(active->key(FacePopupKey::Accept)&&choices==1);s.busy=false;s.error=true;C(active->update(s,style,1280,960,true));C(QMetaObject::invokeMethod(c0,"pressed",Qt::DirectConnection));C(choices==2&&selected==0);s.current=2;C(active->update(s,style,1280,960,true)&&active->selection()==2);s.offAllowed=false;C(active->update(s,style,1280,960,true));C(active->key(FacePopupKey::Accept)&&choices==2);C(active->key(FacePopupKey::Next)&&active->selection()==2);C(active->key(FacePopupKey::Previous)&&active->selection()==1);wait();C(win.grabWindow().save(QString::fromLocal8Bit(argv[2])+"/face-popup.png"));s.error=false;s.busy=true;C(active->update(s,style,1280,960,true));wait();C(win.grabWindow().save(QString::fromLocal8Bit(argv[2])+"/face-popup-busy.png"));active->applied();C(active->finished());active->close();active=nullptr;C(win.contentItem()->childItems().empty());
 s.busy=false;s.offAllowed=true;s.current=0;active=create();C(active->update(s,style,1280,960,false));C(QMetaObject::invokeMethod(static_cast<QObject*>(active->item())->findChild<QObject*>("X2dNativeFaceChoice0"),"pressed",Qt::DirectConnection));C(choices==2);closeOnChoice=true;C(active->key(FacePopupKey::Accept));C(!active&&choices==3);closeOnChoice=false;C(win.contentItem()->childItems().empty());
 active=create();delete static_cast<QObject*>(active->item());C(!active->item()&&active->finished());active->close();active=nullptr;C(win.contentItem()->childItems().empty());
 active=create();s.canChange=false;C(active->update(s,style,1280,960,true)&&active->finished());active->close();active=nullptr;s.canChange=true;
 active=create();C(!active->update(s,style,400,200,true)&&active->finished());active->close();active=nullptr;
 active=create();wait();auto clicks=choices;QTest::mouseClick(&win,Qt::LeftButton,Qt::NoModifier,QPoint(30,30));wait();fprintf(stderr,"OUTSIDE finished=%d calls=%u prev=%u visible=%d enabled=%d\n",active->finished(),choices,clicks,static_cast<QQuickItem*>(active->item())->isVisible(),static_cast<QQuickItem*>(active->item())->isEnabled());C(active->finished()&&choices==clicks+1);active->close();active=nullptr;
 unsigned failed=0;for(int i=0;i<32;i++){budget=i;hits=0;auto*v=FacePopupView::create(api,stock,sig,profile(),win.contentItem(),win.contentItem(),choose,nullptr);budget=-1;if(hits){++failed;C(!v);}else{C(v);v->close();}C(win.contentItem()->childItems().empty());}C(failed>5);
 for(int i=0;i<100;i++){active=create();C(active->key(FacePopupKey::Abort)&&active->finished());active->close();active=nullptr;C(win.contentItem()->childItems().empty());}
 printf("FACE_POPUP_PASS assertions=%u actual_malloc_failure_positions=%u\n",checks,failed);
}
