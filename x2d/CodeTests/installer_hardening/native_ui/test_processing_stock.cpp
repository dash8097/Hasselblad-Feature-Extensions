#include "processing_stock_view.hpp"
#include "processing_host.hpp"
#include <QQmlComponent>
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_key_profile.hpp"
#include <QKeyEvent>
#include "qt_abi_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QVariant>
#include <QFont>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <cstdio>
#include <cstdlib>
static int checks,closes,finishes;static void check(bool v,const char*n){++checks;if(!v){fprintf(stderr,"PROCESSING_STOCK_FAIL %s\n",n);exit(1);}}
static void action(void*,x2dui::ProcessingAction a){if(a==x2dui::ProcessingAction::Finish)++finishes;else ++closes;}
static void click(QQuickMouseArea*m){QQuickMouseEvent e;m->clicked(&e);}
int main(int argc,char**argv){QGuiApplication app(argc,argv);check(argc==2&&QResource::registerResource(QString::fromLocal8Bit(argv[1])),"fixed-original-private-fixture");auto stock=qt_stock_profile();auto menu=qt_menu_profile();auto sigprofile=qt_signal_profile();auto abi=qt_abi_profile();auto kp=qt_key_profile();QObject mapping;void*instance=&mapping;x2dui::KeyMapProfile km{&instance,&QObject::staticMetaObject,[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));},[](const void*,int key,unsigned,int fn){return fn==5&&key==Qt::Key_Escape;},[](const void*,int key,unsigned){return key==Qt::Key_F1;}};QQmlEngine engine;QQuickWindow window;window.resize(800,600);auto*host=new QQuickItem(window.contentItem());QQmlEngine::setContextForObject(host,engine.rootContext());
 auto*v=x2dui::ProcessingStockView::create(stock,menu,&abi,sigprofile,host,host,x2dui::stock_create(stock,x2dui::StockKind::ExposeViewModel,host,host),action,nullptr,&kp,&km);check(v,"original-page-native-controller-created");auto*root=static_cast<QQuickItem*>(v->item());auto*t=root->findChild<QQuickMouseArea*>("PixelOverclockReturnTouch");auto*p=root->findChild<QObject*>("PixelOverclockNativeProgress");auto*c=root->findChild<QQuickItem*>("PixelOverclockNativeCountdown");check(t&&p&&c,"native-owned-components");
 x2dui::ProcessingState s;s.present=true;s.ready=true;check(v->update(s,800,600),"idle-update");app.processEvents();check(root->isVisible()&&t->QQuickItem::isEnabled()&&t->width()==400&&t->y()==480&&t->height()==120,"touch-uses-real-bottom-area");
 click(t);check(closes==1&&!finishes,"idle-return");s.busy=true;s.stage=x2dui::ProcessingStage::Render;s.renderTiles=3;check(v->update(s,800,600),"render-update");check(!t->QQuickItem::isEnabled()&&!root->property("showButtons").toBool(),"busy-no-return");click(t);check(closes==1,"busy-synthetic-click-rejected");
 auto*first=static_cast<QObject*>(x2dui::stock_get_object(stock,p,"text1"));auto*second=static_cast<QObject*>(x2dui::stock_get_object(stock,p,"text2"));check(first&&second&&first->property("font").value<QFont>().weight()==QFont::Normal&&second->property("font").value<QFont>().weight()==QFont::DemiBold,"native-font-size-preserves-original-weights");
 auto*detail=static_cast<QObject*>(x2dui::stock_get_object(stock,p,"text3"));check(detail&&detail->property("font").value<QFont>().pixelSize()==28,"detail-font-does-not-inherit-large-counter-size");
 s.stack=true;s.stage=x2dui::ProcessingStage::Capture;s.stackRemaining=12;s.stackFrames=5;check(v->update(s,800,600),"stack-capture-update");click(t);check(finishes==1&&closes==1,"finish-only-when-allowed");check(QMetaObject::invokeMethod(root,"abortPressed"),"original-abort-signal");check(finishes==2,"original-abort-mapped");
 s.stage=x2dui::ProcessingStage::Delay;s.countdown=u"3";check(v->update(s,800,600),"countdown-update");check(c->isVisible()&&!static_cast<QQuickItem*>(p)->isVisible(),"only-countdown-visible");
 check(t->QQuickItem::isEnabled()&&root->property("showButtons").toBool(),"countdown-cancel-uses-original-button");click(t);check(finishes==3&&closes==1,"countdown-cancel-dispatched-without-closing-active-task");
 s.busy=false;s.stage=x2dui::ProcessingStage::Complete;check(v->update(s,800,600),"complete-update");check(p->property("progressValue").toDouble()==1,"complete-progress");window.show();root->forceActiveFocus();app.processEvents();QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QGuiApplication::sendEvent(&window,&escape);check(escape.isAccepted()&&closes==2,"native-physical-escape-returns");QKeyEvent pass(QEvent::KeyPress,Qt::Key_F1,Qt::NoModifier);QGuiApplication::sendEvent(&window,&pass);check(!pass.isAccepted()&&closes==2,"unblockable-key-passes-original-filter");
 auto*model=static_cast<QObject*>(x2dui::stock_get_object(stock,root,"viewModel"));check(model&&model->setProperty("sideOriented",true),"orientation-fixture");app.processEvents();check(v->update(s,800,600),"side-layout-update");check(qAbs(t->x()-(800-217.6))<1e-9&&t->y()==300&&qAbs(t->width()-217.6)<1e-9&&t->height()==300,"side-touch-real-background-width");
 check(!v->update(s,0,600),"invalid-geometry-rejected");delete host;check(!v->item()&&!v->update(s,800,600),"parent-destruction-invalidates-view");v->close();
 host=new QQuickItem(window.contentItem());QQmlEngine::setContextForObject(host,engine.rootContext());v=x2dui::ProcessingStockView::create(stock,menu,&abi,sigprofile,host,host,x2dui::stock_create(stock,x2dui::StockKind::ExposeViewModel,host,host),action,nullptr,&kp,&km);check(v,"reopen");check(v->update(s,800,600),"reopen-update");v->close();check(!host->findChild<QQuickItem*>("PixelOverclockPage"),"explicit-close-preserves-borrowed-model");delete host;
 QQmlEngine::setContextForObject(window.contentItem(),engine.rootContext());QQmlComponent component(&engine);component.setData("import QtQuick\nItem {objectName: \"MainViewModel_root\";property QtObject exposeScreen:null}",QUrl());auto*main=static_cast<QQuickItem*>(component.create());check(main,"main-model-fixture");main->setParent(window.contentItem());main->setParentItem(window.contentItem());auto*external=x2dui::stock_create(stock,x2dui::StockKind::ExposeViewModel,window.contentItem(),window.contentItem());check(x2dui::stock_set_object(stock,main,"exposeScreen",external),"existing-exposure-alias");
 x2dui::FocusProfile fp{[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->window();},[](const void*w)->void*{return static_cast<const QQuickWindow*>(w)->activeFocusItem();},[](void*o,int reason){static_cast<QQuickItem*>(o)->forceActiveFocus(static_cast<Qt::FocusReason>(reason));}};
 auto*oldFocus=new QQuickItem(window.contentItem());oldFocus->forceActiveFocus();app.processEvents();x2dui::ProcessingHost attachment(stock,menu,&abi,sigprofile,kp,km,fp,action,nullptr);check(attachment.sync(window.contentItem(),true,s,800,600),"host-reads-existing-main-model");check(window.activeFocusItem()==attachment.item(),"host-takes-processing-focus");check(x2dui::stock_get_object(stock,attachment.item(),"viewModel")==external,"borrowed-original-model-identity");attachment.close(window.contentItem());check(window.activeFocusItem()==oldFocus,"host-restores-previous-live-focus");check(attachment.sync(window.contentItem(),true,s,800,600),"host-reopen");delete oldFocus;attachment.close(window.contentItem());check(!attachment.item(),"dead-return-focus-never-dereferenced");
 check(attachment.sync(window.contentItem(),true,s,800,600),"host-reopen-for-model-deletion");stock.object_destroy(external);check(!attachment.sync(window.contentItem(),true,s,800,600)&&!attachment.item(),"destroyed-factory-model-fails-closed");delete main;
 printf("NATIVE_PROCESSING_STOCK_PASS assertions=%d original_resources=4.2.0 hardware_proxies=fixtures camera_requests=0 Qt_keys_tested=1 camera_KeyFn=fixture\n",checks);
}
