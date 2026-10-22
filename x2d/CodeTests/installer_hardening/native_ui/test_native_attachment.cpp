#include "native_attachment.hpp"
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtCore/QLibrary>
#include <QtCore/QEventLoop>
#include <thread>
#include <cstdio>
#include <cstdlib>
class TestItem final: public QQuickItem { public: using QQuickItem::QQuickItem; ~TestItem() override = default; };
static int checks;
static void check(bool x,const char*n){++checks;fprintf(stderr,"CHECK %d %s\n",checks,n);fflush(stderr);if(!x){fprintf(stderr,"NATIVE_ATTACH_FAIL %s\n",n);std::exit(1);}}
static QQuickItem*host(QQuickWindow&w){auto*p=new TestItem(w.contentItem());p->setObjectName("ControlDrawer_root");for(const auto*n:{"MainScreen_grid","menu_loader","MainMenu_swipeArea","ControlDrawer_list"}){auto*c=new TestItem(p);c->setObjectName(n);}return p;}
static QQuickItem*nullFactory(QQuickItem*,bool){return nullptr;}
static QQuickItem*wrongFactory(QQuickItem*p,bool){return new TestItem(p);}
int main(int argc,char**argv){
 QGuiApplication app(argc,argv);using A=NativeUiAttachment;
 check(QString::fromLatin1(qVersion())=="6.4.1","fixed-Qt");QQuickWindow window;auto*p=host(window);
 check(A::attach(nullptr,false)==A::InvalidHost,"null-host");
 int result=-1;std::thread worker([&]{result=A::attach(p,false);});worker.join();check(result==A::WrongThread,"worker-refused-before-host-touch");
 auto count=p->children().size();check(A::attach(p,false,nullFactory)==A::FactoryFailed,"failed-factory");check(p->children().size()==count,"null-factory-rollback");
 check(A::attach(p,false,wrongFactory)==A::FactoryFailed,"wrong-factory");check(p->children().size()==count,"wrong-factory-rollback");
 check(A::attach(p,false)==A::Attached,"attach");auto*effect=p->findChild<EffectPreferences*>();check(effect&&!effect->networkEnabled()&&effect->parentItem()==p,"owned-disabled-component");QPointer<QQuickItem> guard(effect);
 check(A::attach(p,true)==A::AlreadyAttached&&!effect->networkEnabled(),"duplicate-cannot-enable-network");
 check(A::discoverAndAttach(false)==A::AlreadyAttached,"discover-existing");
 check(A::detach(p)==A::Attached&&guard.isNull(),"detach-destroys-owned");check(p->children().size()==count,"detach-preserves-stock-children");check(A::detach(p)==A::NotAttached,"repeat-detach");
 auto*collision=new TestItem(p);collision->setObjectName("X2dNativeEffectPreferences");check(A::attach(p,false)==A::AmbiguousHost,"foreign-name-collision");delete collision;
 auto*duplicate=new TestItem(p);duplicate->setObjectName("menu_loader");check(A::attach(p,false)==A::InvalidHost,"duplicate-stock-anchor");delete duplicate;
 QQuickWindow second;auto*other=host(second);check(A::discoverAndAttach(false)==A::AmbiguousHost,"multiple-windows-refused");delete other;
 check(A::attach(p,false)==A::Attached,"reattach");guard=p->findChild<EffectPreferences*>();delete p;check(guard.isNull(),"host-destruction-cleans-owned");
 p=host(window);QEventLoop loop;QTimer::singleShot(0,&app,[&]{result=A::discoverAndAttach(false);loop.quit();});loop.exec();check(result==A::Attached,"existing-event-loop-queued-attach");check(A::detach(p)==A::Attached,"queued-attach-cleanup");
 check(argc==2,"dll-path");QLibrary library(QString::fromLocal8Bit(argv[1]));check(library.load(),"dll-load");using Attach=int(*)(QQuickItem*,bool);using Detach=int(*)(QQuickItem*);auto attach=reinterpret_cast<Attach>(library.resolve("x2d_native_attach_v1"));auto detach=reinterpret_cast<Detach>(library.resolve("x2d_native_detach_v1"));check(attach&&detach,"resolve-c-abi");check(attach(p,false)==A::Attached,"dll-attach-existing-runtime");guard=p->findChild<QQuickItem*>("X2dNativeEffectPreferences");check(guard&&guard->parentItem()==p,"dll-visual-parent");check(detach(p)==A::Attached&&guard.isNull(),"dll-synchronous-detach");check(library.unload(),"dll-unload-after-detach");delete p;
 printf("NATIVE_ATTACH_PASS assertions=%d qt=%s network_requests=0 qml_documents=0\n",checks,qVersion());return 0;
}
