#include "screen_controls_view.hpp"
#include "screen_notice_view.hpp"
#include "qt_stock_profile.hpp"
#include "qt_abi_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QImage>
#include <QQuickImageProvider>
#include <QFile>
#include <QFontDatabase>
#include <QRawFont>
#include <QThread>
#include <QElapsedTimer>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned checks,choices;static ScreenControlView*active;static bool closeOnChoice=false,acceptChoice=true;static int budget=-1,allocHits=0;
extern "C" void*__real_malloc(size_t);
extern "C" void*__wrap_malloc(size_t n){if(budget>=0){if(budget--==0){++allocHits;return nullptr;}}return __real_malloc(n);}
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"SCREEN_VIEW_FAIL line=%d\n",__LINE__);exit(1);}}while(0)
static bool changed(void*,int){++choices;if(closeOnChoice){auto*v=active;active=nullptr;v->close();}return acceptChoice;}
static bool setter(void*,const ScreenWrite&){return true;}
static void wait(){QElapsedTimer t;t.start();do{QCoreApplication::processEvents();QThread::msleep(1);}while(t.elapsed()<80);}
static unsigned pixels(const QImage&i,int x,int y,int w,int h){unsigned n=0;for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)if(qGray(i.pixel(xx,yy))>20)++n;return n;}
class Icons final:public QQuickImageProvider{public:Icons():QQuickImageProvider(QQuickImageProvider::Image){}QImage requestImage(const QString&id,QSize*size,const QSize&requested)override{QFile file(":/icons/"+id+".svg");if(!file.open(QIODevice::ReadOnly))return{};QImage image=QImage::fromData(file.readAll(),"SVG");if(image.isNull())return{};if(requested.width()>0)image=image.scaledToWidth(qRound(image.width()*requested.width()/100.0),Qt::SmoothTransformation);*size=image.size();return image;}};
int main(int argc,char**argv){QGuiApplication app(argc,argv);C(argc==3);C(QFontDatabase::addApplicationFont(QString::fromLocal8Bit(argv[2])+"/private-font.ttc")>=0);QFont font("Microsoft YaHei");font.setPixelSize(28);app.setFont(font);C(QRawFont::fromFont(font).supportsCharacter(0x663e));C(QResource::registerResource(QString::fromLocal8Bit(argv[1])));C(QResource::registerResource(QString::fromLocal8Bit(argv[2])+"/extra.rcc"));C(!QImage(":/icons/ic_menu_switch_close.svg").isNull());QQmlEngine engine;engine.addImageProvider("svg",new Icons);QQuickWindow win;win.resize(800,600);win.setColor(Qt::black);QQmlEngine::setContextForObject(win.contentItem(),engine.rootContext());auto abi=qt_abi_profile();auto stock=qt_stock_profile();auto api=abi_view_api(&abi);ScreenControlStyle style{88,30,30,28,28,22,0,400,40,132,132,4.3};const char16_t*names[]={u"自动",u"屏幕",u"取景器"};ScreenControl model(ScreenControlKind::Display,{nullptr,setter});ScreenControlSnapshot state;state.enabled=true;state.labels=names;state.count=3;state.current=names[0];state.backBit=2;state.evfBit=8;state.screensAllowed=2;C(model.update(state));
 auto create=[&](){auto*v=ScreenControlView::create(api,stock,&abi,&model,win.contentItem(),win.contentItem(),win.contentItem(),changed,nullptr);C(v);C(v->update(style,800,600,0,true));return v;};active=create();win.show();C(active->key(ScreenControlKey::Open)&&active->popup_open());C(active->key(ScreenControlKey::Down)&&active->selection()==1);
 auto*animated=win.contentItem()->findChild<QQuickItem*>("X2dScreenChoice3");C(animated);double startY=animated->y();active->advance(100);active->advance(116);C(animated->y()<startY&&animated->y()>startY-((600-264)/4.3)*.5);
 C(active->update(style,800,600,0,true)&&active->selection()==1);C(active->key(ScreenControlKey::Down)&&active->selection()==2);C(active->key(ScreenControlKey::Down)&&active->selection()==3);C(active->key(ScreenControlKey::Down)&&active->selection()==3);active->advance(1000);for(unsigned t=1016;t<1500;t+=16)active->advance(t);wait();C(win.grabWindow().save(QString::fromLocal8Bit(argv[2])+"/screen-selector.png"));
 acceptChoice=false;C(active->key(ScreenControlKey::Accept)&&active->popup_open()&&choices==1);acceptChoice=true;C(active->key(ScreenControlKey::Accept)&&!active->popup_open()&&choices==2);C(active->key(ScreenControlKey::Open));C(active->update(style,800,600,0,false));auto*m=win.contentItem()->findChild<QQuickMouseArea*>("X2dScreenChoice3");C(m);QQuickMouseEvent e;m->pressed(&e);m->released(&e);C(choices==2);C(active->key(ScreenControlKey::Accept)&&choices==3);C(!active->popup_open());
 C(active->update(style,800,600,0,true));C(active->key(ScreenControlKey::Open));closeOnChoice=true;C(active->key(ScreenControlKey::Accept));C(!active&&choices==4);closeOnChoice=false;C(win.contentItem()->childItems().empty());
 active=create();auto*root=static_cast<QQuickItem*>(active->item());delete root;C(!active->item()&&!active->update(style,800,600,0,true));active->close();active=nullptr;C(win.contentItem()->childItems().empty());
 active=create();auto*popup=win.contentItem()->findChild<QQuickItem*>("X2dNativeScreenSelector");C(popup);delete popup;C(!active->update(style,800,600,0,true));active->key(ScreenControlKey::Open);C(!active->popup_open());active->close();active=nullptr;C(win.contentItem()->childItems().empty());
 unsigned failures=0;for(int fail=0;fail<40;fail++){budget=fail;allocHits=0;auto*v=ScreenControlView::create(api,stock,&abi,&model,win.contentItem(),win.contentItem(),win.contentItem(),changed,nullptr);budget=-1;if(allocHits){++failures;C(!v);}else{C(v);v->close();}C(win.contentItem()->childItems().empty());}C(failures>20);
 ScreenControl fps(ScreenControlKind::AdaptiveFps,{nullptr,setter});ScreenControlSnapshot f;f.enabled=true;C(fps.update(f));active=ScreenControlView::create(api,stock,&abi,&fps,win.contentItem(),win.contentItem(),win.contentItem(),changed,nullptr);C(active);style.descriptionHeight=90;C(active->update(style,800,600,0,true));C(static_cast<QQuickItem*>(active->item())->findChild<QQuickItem*>("X2dNativeScreenDescription")->opacity()==.7);wait();auto fpsImage=win.grabWindow();C(pixels(fpsImage,30,0,250,88)>300);C(pixels(fpsImage,640,0,140,88)>300);C(fpsImage.save(QString::fromLocal8Bit(argv[2])+"/screen-adaptive-fps.png"));auto*originalSwitch=static_cast<QQuickItem*>(active->item())->findChild<QQuickItem*>("MenuBoolSelector_root");C(originalSwitch&&originalSwitch->property("showSwitch").toBool());style.showSwitch=false;C(active->update(style,800,600,0,true));C(!originalSwitch->property("showSwitch").toBool());win.update();wait();auto checkboxImage=win.grabWindow();C(pixels(checkboxImage,30,0,250,88)>300);C(pixels(checkboxImage,700,0,80,88)>30);C(checkboxImage.save(QString::fromLocal8Bit(argv[2])+"/screen-adaptive-checkbox.png"));f.fps=true;C(fps.update(f));style.showSwitch=true;C(active->update(style,800,600,0,true));win.update();wait();auto enabledImage=win.grabWindow();C(pixels(enabledImage,30,0,250,88)>300);C(originalSwitch->property("value").toBool());C(enabledImage.save(QString::fromLocal8Bit(argv[2])+"/screen-adaptive-fps-enabled.png"));auto n=choices;C(active->key(ScreenControlKey::Open)&&choices==n+1&&!active->popup_open());active->close();active=nullptr;
 auto*notice=ScreenNoticeView::create(api,stock,win.contentItem(),win.contentItem());C(notice);C(notice->update(true,800,600,u"仅在取景器中显示实时取景"));C(static_cast<QQuickItem*>(notice->item())->isVisible());win.update();wait();auto noticeImage=win.grabWindow();C(pixels(noticeImage,200,180,400,260)>500);C(noticeImage.save(QString::fromLocal8Bit(argv[2])+"/screen-notice.png"));C(notice->update(false,800,600,u"仅在取景器中显示实时取景")&&!static_cast<QQuickItem*>(notice->item())->isVisible());C(!notice->update(true,-1,600,u"invalid"));delete static_cast<QQuickItem*>(notice->item());C(!notice->item()&&!notice->update(true,800,600,u"invalid"));notice->close();C(win.contentItem()->childItems().empty());
 unsigned noticeFail=0;for(int i=0;i<8;i++){budget=i;allocHits=0;notice=ScreenNoticeView::create(api,stock,win.contentItem(),win.contentItem());budget=-1;if(allocHits){++noticeFail;C(!notice);}else{C(notice);notice->close();}C(win.contentItem()->childItems().empty());}C(noticeFail>=1);
 printf("SCREEN_VIEW_PASS assertions=%u actual_allocation_failure_positions=%u notice_allocation_failure_positions=%u\n",checks,failures,noticeFail);
}
