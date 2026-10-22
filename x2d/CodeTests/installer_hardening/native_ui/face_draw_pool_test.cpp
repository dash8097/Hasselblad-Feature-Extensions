#include "face_draw_pool.hpp"
#include "qt_stock_profile.hpp"
#include "qt_abi_profile.hpp"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QImage>
#include <QFontDatabase>
#include <QQmlEngine>
#include <QQmlContext>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace x2dui;
static unsigned checks;static int budget=-1,hits=0;
extern "C" void*__real_malloc(size_t);
extern "C" void*__wrap_malloc(size_t n){if(budget>=0&&budget--==0){++hits;return nullptr;}return __real_malloc(n);}
extern "C" void*__real_calloc(size_t,size_t);
extern "C" void*__wrap_calloc(size_t n,size_t z){if(budget>=0&&budget--==0){++hits;return nullptr;}return __real_calloc(n,z);}
#define C(x) do{++checks;if(!(x)){fprintf(stderr,"DRAW_POOL_FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static void wait(){QElapsedTimer t;t.start();do{QCoreApplication::processEvents();QThread::msleep(1);}while(t.elapsed()<80);}
int main(int argc,char**argv){QGuiApplication app(argc,argv);C(argc==2);C(QFontDatabase::addApplicationFont(QString::fromLocal8Bit(argv[1])+"/private-font.ttc")>=0);app.setFont(QFont("Microsoft YaHei"));QQmlEngine engine;QQuickWindow w;QQmlEngine::setContextForObject(w.contentItem(),engine.rootContext());w.resize(320,240);w.setColor(Qt::black);auto abi=qt_abi_profile();auto api=abi_view_api(&abi);auto stock=qt_stock_profile();FaceColorProfile colors{QMetaType::fromType<QColor>().iface(),abi.font_size};
 FaceFrame f;f.valid=true;f.width=320;f.height=240;f.count=3;f.commands[0]={FaceDrawKind::Rectangle,20,20,80,60,0,4,0,0x00ff00ff};f.commands[1]={FaceDrawKind::RoundedRectangle,120,20,80,60,12,3,0x222222ff,0xffffffff};f.commands[2]={FaceDrawKind::Text,20,100,260,40,0,0,0xffffffff,0,u"FACE / EYE",10,24};
 auto create=[&](){auto*p=FaceDrawPool::create(api,stock,colors,w.contentItem());C(p);return p;};auto*p=create();bool updated=p->update(f);fprintf(stderr,"UPDATED=%d nodes=%u\n",updated,p->nodes());for(auto*child:static_cast<QQuickItem*>(p->item())->childItems())fprintf(stderr,"NODE class=%s text=%s width=%f\n",child->metaObject()->className(),qPrintable(child->property("text").toString()),child->width());C(updated);C(p->nodes()==3);auto*r=static_cast<QQuickItem*>(p->item());C(r->clip()&&r->isVisible());auto items=r->childItems();C(items.size()==3);C(items[0]->property("color").value<QColor>().alpha()==0);auto*pen=items[0]->property("border").value<QObject*>();C(pen&&pen->property("width")==4&&pen->property("color").value<QColor>()==QColor(Qt::green));C(items[1]->property("radius")==12);C(items[2]->property("text")=="FACE / EYE");w.show();wait();auto img=w.grabWindow();C(!img.isNull());C(img.pixelColor(20,25).green()>200);C(img.pixelColor(35,35)==QColor(Qt::black));C(img.pixelColor(121,21)==QColor(Qt::black));C(img.save(QString::fromLocal8Bit(argv[1])+"/face-draw-pool.png"));
 delete items[1];C(p->update(f));C(r->childItems().size()==3);f.count=1;C(p->update(f));unsigned visible=0;for(auto*i:r->childItems())visible+=i->isVisible();C(visible==1);f.commands[0]=f.commands[2];C(p->update(f));C(r->childItems().size()==3);f.commands[0].x=std::numeric_limits<double>::infinity();C(!p->update(f)&&!r->isVisible());f.commands[0].x=1;f.count=561;C(!p->update(f));f.count=1;C(p->update(f));delete r;C(!p->item()&&!p->update(f));p->close();C(w.contentItem()->childItems().empty());
 unsigned faults=0;for(int i=0;i<14;i++){budget=i;hits=0;p=FaceDrawPool::create(api,stock,colors,w.contentItem());bool ok=p&&p->update(f);budget=-1;fprintf(stderr,"BUDGET %d hits=%d ok=%d children=%lld\n",i,hits,ok,(long long)w.contentItem()->childItems().size());if(hits){++faults;C(!ok);}if(p)p->close();C(w.contentItem()->childItems().empty());}C(faults>=3);
 f.count=560;for(auto&d:f.commands)d={FaceDrawKind::Rectangle,1,1,2,2,0,0,0xffffffff,0};p=create();C(p->update(f)&&p->nodes()==560);for(int i=0;i<20;i++){C(p->update(f));}C(static_cast<QQuickItem*>(p->item())->childItems().size()==560);p->close();C(w.contentItem()->childItems().empty());printf("FACE_DRAW_POOL_PASS assertions=%u actual_malloc_failures=%u\n",checks,faults);
}
