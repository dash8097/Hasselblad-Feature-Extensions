#include "focus_popup_view.hpp"
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
#include <cstring>
using namespace x2dui;

static unsigned checks,choices,aborts;static int selected=-1;static FocusPopupView*active;static bool closeOnChoice=false,closeOnAbort=false,allow=true;static int budget=-1,hits=0;
extern "C" void*__real_malloc(size_t);extern "C" void*__wrap_malloc(size_t n){if(budget>=0&&budget--==0){++hits;return nullptr;}return __real_malloc(n);}
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"FOCUS_POPUP_FAIL line=%d checks=%u\n",__LINE__,checks);exit(1);}}while(0)
static bool choose(void*,int n){++choices;selected=n;if(closeOnChoice){auto*p=active;active=nullptr;p->close();}return allow;}
static void abortPopup(void*){++aborts;if(closeOnAbort){auto*p=active;active=nullptr;p->close();}}

static void(*realName)(void*,void*,const char*);static void(*realLayer)(void*,void*,double,bool);static bool(*realSet)(void*,const char*,const void*);static int constructFault,constructHits;
static void destructiveName(void*c,void*o,const char*n){realName(c,o,n);const char*names[]={"X2dNativeFocusPopup","X2dFocusContent","X2dFocusStatus","X2dFocusChoice0","X2dFocusIcon0","X2dFocusOutside0"};if(constructFault>=0&&constructFault<6&&!strcmp(n,names[constructFault])){++constructHits;delete static_cast<QObject*>(o);}}
static void destructiveLayer(void*c,void*o,double z,bool clip){realLayer(c,o,z,clip);if(constructFault==6&&z==2100){++constructHits;delete static_cast<QObject*>(o);}if(constructFault==7&&z==101){++constructHits;delete static_cast<QObject*>(o);}}
static bool destructiveSet(void*o,const char*n,const void*v){bool r=realSet(o,n,v);if(constructFault==8&&!strcmp(n,"handleDefaultKeys")){++constructHits;delete static_cast<QObject*>(o);}if(constructFault==9&&!strcmp(n,"acceptKeys")){++constructHits;delete static_cast<QObject*>(o);}return r;}
static void wait(){QElapsedTimer t;t.start();do{QCoreApplication::processEvents();QThread::msleep(1);}while(t.elapsed()<100);}
class Icons final:public QQuickImageProvider{public:Icons():QQuickImageProvider(QQuickImageProvider::Image){}QImage requestImage(const QString&id,QSize*size,const QSize&requested)override{QFile f(":/icons/"+id+".svg");if(!f.open(QIODevice::ReadOnly))return{};auto i=QImage::fromData(f.readAll(),"SVG");if(requested.width()>0)i=i.scaledToWidth(qRound(i.width()*requested.width()/100.),Qt::SmoothTransformation);*size=i.size();return i;}};
static FacePopupProfile profile(){static_assert(sizeof(QQmlListProperty<QQuickItem>)==sizeof(FacePopupList));return {QMetaType::fromType<QQmlListProperty<QQuickItem>>().iface()};}


int main(int argc,char**argv){QGuiApplication app(argc,argv);C(argc==3);C(QFontDatabase::addApplicationFont(QString::fromLocal8Bit(argv[2])+"/private-font.ttc")>=0);app.setFont(QFont("Microsoft YaHei"));C(QResource::registerResource(QString::fromLocal8Bit(argv[1])));C(QResource::registerResource(QString::fromLocal8Bit(argv[2])+"/extra.rcc"));QQmlEngine engine;engine.addImageProvider("svg",new Icons);QQuickWindow win;win.resize(1280,960);win.setColor(Qt::black);QQmlEngine::setContextForObject(win.contentItem(),engine.rootContext());auto abi=qt_abi_profile();auto stock=qt_stock_profile();auto api=abi_view_api(&abi);auto sig=qt_signal_profile();FocusPopupState s;s.source.valid=s.source.selectable=true;s.source.mode=1;s.selected=0;for(auto&v:s.source.allowed)v=true;FocusPopupStyle style;
 auto create=[&](){auto*v=FocusPopupView::create(api,stock,sig,profile(),win.contentItem(),win.contentItem(),choose,abortPopup,nullptr);C(v);C(v->update(s,style,1280,960,true));return v;};
 active=create();win.show();C(active->selection()==0&&!active->finished());auto*root=static_cast<QQuickItem*>(active->item());C(!root->property("handleDefaultKeys").toBool());auto*c0=root->findChild<QQuickItem*>("X2dFocusChoice0");C(c0&&c0->parentItem()->width()==672&&c0->width()==208&&c0->height()==210);C(c0->property("highlightWidth").toDouble()==146&&c0->property("highlightHeight").toDouble()==96);C(!c0->property("acceptKeys").toBool());
 for(int lang=0;lang<11;lang++){s.source.language=lang;C(active->update(s,style,1280,960,true));for(int i=0;i<3;i++){auto*o=root->findChild<QObject*>(QString("X2dFocusChoice%1").arg(i));C(o->property("text").toString()==QString::fromUtf16(focus_text(lang,i)));}s.pending=2;C(active->update(s,style,1280,960,true));C(root->findChild<QObject*>("X2dFocusStatus")->property("text").toString()==QString::fromUtf16(focus_text(lang,3)));s.pending=-1;s.failed=true;C(active->update(s,style,1280,960,true));C(root->findChild<QObject*>("X2dFocusStatus")->property("text").toString()==QString::fromUtf16(focus_text(lang,4)));s.failed=false;}
 s.source.language=8;C(active->update(s,style,1280,960,true));C(active->key(FocusKey::Next)&&active->selection()==1);C(active->key(FocusKey::Accept)&&choices==1&&selected==1&&!active->finished());for(int i=0;i<20;i++){C(active->key(FocusKey::Accept));C(active->update(s,style,1280,960,true));}C(choices==1);s.pending=2;C(active->update(s,style,1280,960,true));C(active->key(FocusKey::Next)&&active->selection()==1);s.pending=-1;s.failed=true;C(active->update(s,style,1280,960,true));allow=false;C(active->key(FocusKey::Accept)&&choices==2);C(active->key(FocusKey::Accept)&&choices==3);allow=true;s.failed=false;s.source.allowed[1]=false;C(active->update(s,style,1280,960,true));C(active->key(FocusKey::Accept)&&choices==3);C(active->key(FocusKey::Next)&&active->selection()==2);wait();C(win.grabWindow().save(QString::fromLocal8Bit(argv[2])+"/focus-popup.png"));C(active->key(FocusKey::Abort)&&active->finished()&&aborts==1&&choices==3);active->close();active=nullptr;C(win.contentItem()->childItems().empty());
 s.source.allowed[1]=true;s.selected=0;active=create();C(active->update(s,style,1280,960,false));C(QMetaObject::invokeMethod(static_cast<QObject*>(active->item())->findChild<QObject*>("X2dFocusChoice1"),"pressed",Qt::DirectConnection));C(choices==3);closeOnChoice=true;C(active->key(FocusKey::Accept));C(!active&&choices==4);closeOnChoice=false;C(win.contentItem()->childItems().empty());
 active=create();wait();auto old=choices;auto oldAbort=aborts;QTest::mouseClick(&win,Qt::LeftButton,Qt::NoModifier,QPoint(30,30));wait();C(active->finished()&&choices==old&&aborts==oldAbort+1);active->close();active=nullptr;
 active=create();closeOnAbort=true;C(active->key(FocusKey::Abort)&&!active);closeOnAbort=false;C(win.contentItem()->childItems().empty());active=create();delete static_cast<QObject*>(active->item());C(!active->item()&&active->finished());active->close();active=nullptr;C(win.contentItem()->childItems().empty());active=create();C(!active->update(s,style,400,200,true)&&active->finished());active->close();active=nullptr;

 active=create();root=static_cast<QQuickItem*>(active->item());QObject::connect(root,&QQuickItem::widthChanged,&win,[](){auto*p=active;active=nullptr;p->close();});C(!active->update(s,style,1300,960,true)&&!active);C(win.contentItem()->childItems().empty());
 active=create();root=static_cast<QQuickItem*>(active->item());QObject::connect(root,&QQuickItem::widthChanged,&win,[root](){delete root;});C(!active->update(s,style,1300,960,true)&&active->finished());active->close();active=nullptr;C(win.contentItem()->childItems().empty());

 const char*children[]={"X2dFocusContent","X2dFocusStatus","X2dFocusChoice0","X2dFocusChoice1","X2dFocusChoice2","X2dFocusIcon0","X2dFocusIcon1","X2dFocusIcon2","X2dFocusOutside0","X2dFocusOutside1","X2dFocusOutside2","X2dFocusOutside3"};
 for(auto name:children){active=create();auto*r=static_cast<QObject*>(active->item());auto*o=r->findChild<QObject*>(name);C(o);delete o;C(!active->update(s,style,1280,960,true)&&!active->key(FocusKey::Accept));active->close();active=nullptr;C(win.contentItem()->childItems().empty());}

 realName=api.name;realLayer=api.layer;realSet=stock.set;for(constructFault=0;constructFault<10;constructFault++){constructHits=0;auto risky=api;auto riskyStock=stock;risky.name=destructiveName;risky.layer=destructiveLayer;riskyStock.set=destructiveSet;auto*v=FocusPopupView::create(risky,riskyStock,sig,profile(),win.contentItem(),win.contentItem(),choose,abortPopup,nullptr);C(!v&&constructHits==1);C(win.contentItem()->childItems().empty());}constructFault=-1;
 unsigned failed=0;for(int i=0;i<64;i++){budget=i;hits=0;auto*v=FocusPopupView::create(api,stock,sig,profile(),win.contentItem(),win.contentItem(),choose,abortPopup,nullptr);budget=-1;if(hits){++failed;C(!v);}else{C(v);v->close();}C(win.contentItem()->childItems().empty());}C(failed>5);
 for(int i=0;i<100;i++){active=create();C(active->key(FocusKey::Abort)&&active->finished());active->close();active=nullptr;C(win.contentItem()->childItems().empty());}printf("FOCUS_POPUP_PASS assertions=%u actual_malloc_failure_positions=%u languages=11 camera_operations=0\n",checks,failed);
}
