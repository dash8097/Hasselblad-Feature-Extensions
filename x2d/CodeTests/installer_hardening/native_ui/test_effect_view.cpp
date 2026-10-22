#include "effect_view_core.hpp"
#include "qt_view_api.hpp"
#include "qt_abi_profile.hpp"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QPointer>
#include <QFontDatabase>
#include <QtQuick/private/qquicktext_p.h>
#include <QtTest/QTest>
#include <cstdio>
#include <cstdlib>
static int checks,selections,last=-1;
static void check(bool truth,const char*label){++checks;if(!truth){std::fprintf(stderr,"VIEW_FAIL %s\n",label);std::exit(1);}}
static void selected(void*,int value){++selections;last=value;}
static void click(QQuickWindow&w,int x,int y){QTest::mouseClick(&w,Qt::LeftButton,Qt::NoModifier,{x,y});QTest::qWait(20);}
int main(int argc,char**argv){
 QGuiApplication app(argc,argv);int fontId=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");check(fontId>=0,"local-system-font-read-only");app.setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).first()));QQuickWindow window;window.setColor(Qt::black);window.resize(800,600);window.show();check(QTest::qWaitForWindowExposed(&window),"window-exposed");
 auto*host=new QQuickItem(window.contentItem());host->setParent(window.contentItem());host->setWidth(800);host->setHeight(88);
 x2dui::AbiViewProfile profile{};auto api=qt_view_api();if(argc>2){profile=qt_abi_profile();api=x2dui::abi_view_api(&profile);}
 auto*view=x2dui::EffectView::create(api,host,window.contentItem(),selected,nullptr);check(view!=nullptr,"created");view->resize(800,600);
 QPointer<QQuickItem> item(static_cast<QQuickItem*>(view->item()));check(item&&item->objectName()=="X2dExposureEffectOption","named-root");
 click(window,200,40);check(!view->popupOpen()&&!selections,"disabled-no-action");
 view->update(1,true);
 QQuickText*confirmedValue=nullptr;for(auto*c:item->childItems())if(auto*t=qobject_cast<QQuickText*>(c))if(t->text()==QString::fromUtf16(u"动画"))confirmedValue=t;
 check(confirmedValue,"confirmed-animation-value");
 view->update(2,false,true);check(confirmedValue->text()==QString::fromUtf16(u"动画"),"saving-preserves-confirmed-animation-not-requested-value");
 view->update(2,true);check(confirmedValue->text()==QString::fromUtf16(u"动画与声音"),"animation-confirmed-reply-updates-value");
 view->update(1,true);click(window,200,40);check(view->popupOpen()&&view->selection()==1,"open-current-index");
 click(window,500,390);check(!view->popupOpen()&&selections==1&&last==2,"tap-choice-two");
 view->update(0,true);click(window,200,40);check(view->popupOpen(),"reopen");click(window,20,200);check(!view->popupOpen()&&selections==1,"outside-dismiss");
 click(window,200,40);QTest::mousePress(&window,Qt::LeftButton,Qt::NoModifier,{500,300});QTest::mouseMove(&window,{500,200},20);QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,{500,200});QTest::qWait(30);
 check(view->popupOpen()&&view->selection()==1&&selections==1,"vertical-swipe-highlight-without-write");click(window,500,300);check(!view->popupOpen()&&selections==2&&last==1,"tap-highlight-to-select");
 view->update(2,true);click(window,200,40);check(view->popupOpen(),"open-before-disable");view->update(2,false);check(!view->popupOpen(),"saving-closes-popup");
 view->update(1,true);click(window,200,40);host->setVisible(false);check(!view->popupOpen(),"parent-hide-closes");host->setVisible(true);
 view->resize(640,480);check(item->width()==640,"resize-width");view->resize(800,600);click(window,200,40);QTest::qWait(80);
 if(argc>1){auto image=window.grabWindow();check(!image.isNull()&&image.save(QString::fromLocal8Bit(argv[1])),"render-image");}
 delete host;check(!item,"host-destroys-root");check(!window.contentItem()->findChild<QQuickItem*>("X2dNativeEffectPopup"),"host-destroys-popup");view->close();
 QTest::qWait(10);check(selections==2,"no-action-on-destruction");
 static const char16_t*const modes[]={u"关",u"14位普通",u"14位HSS",u"16位普通",u"16位HSS"};
 host=new QQuickItem(window.contentItem());host->setParent(window.contentItem());host->setWidth(800);host->setHeight(88);
 view=x2dui::EffectView::createChoices(api,host,window.contentItem(),selected,nullptr,u"电子快门引闪",modes,5,"X2dNativeFlashOption");check(view!=nullptr,"flash-created");view->resize(800,600);view->update(4,true);
 click(window,200,40);check(view->popupOpen()&&view->selection()==4,"flash-five-options-current");click(window,500,300);check(!view->popupOpen()&&last==4&&selections==3,"flash-select-sixteen-hss");
 view->update(5,true);click(window,200,40);check(!view->popupOpen()&&selections==3,"flash-invalid-choice-disabled");view->update(1,true);click(window,200,40);host->setVisible(false);check(!view->popupOpen()&&selections==3,"flash-hide-does-not-disable-output");
 delete host;view->close();check(selections==3,"flash-destroy-no-command");
 std::printf("NATIVE_EFFECT_VIEW_PASS assertions=%d QML=0 transport=fake camera_requests=0\n",checks);return 0;
}
