#define main face_profile_unit_main
#include "face_live_profile_test.cpp"
#undef main
#include "face_live_attachment.hpp"
#include "qt_binding_profile.hpp"
#include "qt_abi_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_key_profile.hpp"
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QFontDatabase>
#include <QtTest/QTest>
extern "C" int x2d_face_io_submit(X2dFaceIo*,const X2dFaceRequest*){return 0;}
extern "C" int x2d_face_io_poll(X2dFaceIo*,X2dFaceReply*){return 0;}
extern "C" int x2d_face_io_cancel(X2dFaceIo*,uint64_t){return 0;}
static unsigned stops=0,releases=0;static char fakeIO;
extern "C" void x2d_face_io_stop(X2dFaceIo*){++stops;}
extern "C" int x2d_face_io_release(X2dFaceIo*){++releases;return 1;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);C(argc==3);C(QResource::registerResource(QString::fromLocal8Bit(argv[1])));C(QResource::registerResource(QString::fromLocal8Bit(argv[2])+"/extra.rcc"));C(QFontDatabase::addApplicationFont(QString::fromLocal8Bit(argv[2])+"/private-font.ttc")>=0);app.setFont(QFont("Microsoft YaHei"));QQmlEngine e;QQuickWindow w;w.resize(1280,960);w.setColor(Qt::black);QQmlEngine::setContextForObject(w.contentItem(),e.rootContext());auto abi=qt_abi_profile();auto api=abi_view_api(&abi);auto stock=qt_stock_profile();auto menu=qt_menu_profile();auto binding=qt_binding_profile();auto sig=qt_signal_profile();ScreenKeyProfile keys;keys.key=qt_key_profile();keys.attached_object=[](void*o,void*(*f)(void*),bool c)->void*{return qmlAttachedPropertiesObject(static_cast<QObject*>(o),reinterpret_cast<QQmlAttachedPropertiesFunc>(f),c);};keys.keys_call=reinterpret_cast<AbiMetaCall>(QQuickKeysAttached::staticMetaObject.d.static_metacall);keys.window=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->window();};keys.active=[](const void*o)->void*{return static_cast<const QQuickWindow*>(o)->activeFocusItem();};keys.parent_item=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->parentItem();};void*cam=&camera,*sys=&systemObject;auto f=profile(&cam,&sys);KeyMapProfile map{};map.instance=&cam;map.meta=&QObject::staticMetaObject;map.cast=f.factory.proxy.cast;map.is_fn=[](const void*,int k,unsigned,int fn){return(k==Qt::Key_A&&fn==3)||(k==Qt::Key_B&&fn==2);};map.unblockable=[](const void*,int,unsigned){return false;};auto scene=[&](){QQmlComponent c(&e,QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2])+"/scene.qml"));if(c.isError())fprintf(stderr,"%s\n",qPrintable(c.errorString()));auto*r=qobject_cast<QQuickItem*>(c.create(e.rootContext()));C(r);r->setParent(w.contentItem());r->setParentItem(w.contentItem());return r;};
 auto*r=scene();FaceLiveAttachment a(api,&abi,stock,menu,binding,sig,f,keys,map,reinterpret_cast<X2dFaceIo*>(&fakeIO),10);int rc=a.tick(r,0);fprintf(stderr,"ATTACH rc=%d fail=%u ready=%d\n",rc,a.failures(),a.hooks_ready());C(rc==0&&a.hooks_ready());auto*af=r->findChild<QQuickItem*>("AfIndicator_root");auto*face=r->findChild<QQuickItem*>("FaceIndicator_root");auto*pad=r->findChild<QQuickItem*>("X2dNativeFaceEVFTouchpad");C(af&&face&&pad);C(!face->isVisible()&&!face->property("selectionModeActive").toBool());C(!pad->isVisible());C(a.session().point().snapshot().count==2);w.show();r->forceActiveFocus();QTest::qWait(50);C(a.tick(r,50)==0);C(w.grabWindow().save(QString::fromLocal8Bit(argv[2])+"/face-live.png"));
 // Real MouseArea delivery: preview in pixels; final setter in normalized coordinates.
 auto*drag=qobject_cast<QQuickMouseArea*>(r->findChild<QQuickItem*>("X2dNativeFaceDrag"));C(drag);fprintf(stderr,"DRAG enabled=%d visible=%d w=%f h=%f can=%d\n",drag->isEnabled(),drag->isVisible(),drag->width(),drag->height(),a.session().point().can_drag());QObject::connect(drag,qOverload<QQuickMouseEvent*>(&QQuickMouseArea::pressed),[](auto*){fprintf(stderr,"PRESS DELIVERED\n");});QObject::connect(drag,qOverload<QQuickMouseEvent*>(&QQuickMouseArea::clicked),[](auto*){fprintf(stderr,"CLICK DELIVERED\n");});QTest::mousePress(&w,Qt::LeftButton,Qt::NoModifier,QPoint(500,300));QTest::mouseRelease(&w,Qt::LeftButton,Qt::NoModifier,QPoint(500,300));QTest::qWait(10);C(a.session().point().state().personLocked);C(a.tick(r,100)==0);
 // Face off restores factory bindings; EVF ownership redirects only our Touchpad.
 r->setProperty("faceDetectionMulti",false);C(a.tick(r,150)==0);C(face->isVisible()&&face->property("selectionModeActive").toBool());r->setProperty("faceDetectionMulti",true);r->setProperty("screenBack",false);r->setProperty("screenEVF",true);r->setProperty("evfTpMoveFocus",true);C(a.tick(r,200)==0);C(pad->isVisible()&&pad->isEnabled()&&pad->property("allowDrag").toBool());C(QMetaObject::invokeMethod(pad,"posUpdated",Q_ARG(bool,true),Q_ARG(QPointF,QPointF(20,10))));C(QMetaObject::invokeMethod(pad,"posUpdated",Q_ARG(bool,false),Q_ARG(QPointF,QPointF(0,0))));C(a.tick(r,250)==0);
 a.close();C(a.drain());C(face->isVisible());C(!r->findChild<QObject*>("X2dNativeFaceLayer")&&!r->findChild<QObject*>("X2dNativeFaceEVFTouchpad"));delete r;
 // Loss of one original object must delete surviving owned controls and restore survivors.
 r=scene();FaceLiveAttachment b(api,&abi,stock,menu,binding,sig,f,keys,map,reinterpret_cast<X2dFaceIo*>(&fakeIO),10);C(b.tick(r,0)==0&&b.hooks_ready());delete r->findChild<QObject*>("AfIndicator_root");C(b.tick(r,50)==0&&!b.hooks_ready());C(!r->findChild<QObject*>("X2dNativeFaceLayer")&&!r->findChild<QObject*>("X2dNativeFaceEVFTouchpad"));b.close();C(b.drain());delete r;
 // Tick-gap permission change cannot dispatch the old pointer action.
 r=scene();FaceLiveAttachment guarded(api,&abi,stock,menu,binding,sig,f,keys,map,reinterpret_cast<X2dFaceIo*>(&fakeIO),10);C(guarded.tick(r,0)==0&&guarded.hooks_ready());auto originalWrites=writes;r->setProperty("inIdleState",false);QTest::mouseClick(&w,Qt::LeftButton,Qt::NoModifier,QPoint(500,300));C(writes==originalWrites&&!guarded.session().point().state().personLocked);guarded.close();C(guarded.drain());delete r;
 // Foreign writes are preserved and not automatically leased again on the same source.
 r=scene();FaceLiveAttachment foreign(api,&abi,stock,menu,binding,sig,f,keys,map,reinterpret_cast<X2dFaceIo*>(&fakeIO),10);C(foreign.tick(r,0)==0&&foreign.hooks_ready());face=r->findChild<QQuickItem*>("FaceIndicator_root");face->setVisible(true);C(foreign.tick(r,50)!=0);C(face->isVisible());for(int i=1;i<8;i++){foreign.tick(r,250*i);C(!foreign.hooks_ready()&&face->isVisible());}foreign.close();C(foreign.drain());delete r;
 // Live view may be constructed after the first 20 seconds, under the same root.
 r=scene();af=r->findChild<QQuickItem*>("AfIndicator_root");C(af);af->setObjectName("NotYetAvailable");
 FaceLiveAttachment late(api,&abi,stock,menu,binding,sig,f,keys,map,reinterpret_cast<X2dFaceIo*>(&fakeIO),10);
 for(unsigned i=0;i<100;i++){C(late.tick(r,i*250)==0);C(!late.hooks_ready());}
 af->setObjectName("AfIndicator_root");C(late.tick(r,26000)==0&&late.hooks_ready());late.close();C(late.drain());delete r;
 auto oldStops=stops;{FaceLiveAttachment invalid(api,&abi,stock,menu,binding,sig,f,keys,map,reinterpret_cast<X2dFaceIo*>(&fakeIO),-1);C(stops==oldStops+1);C(invalid.drain());}C(stops==oldStops+1);
 printf("FACE_LIVE_ATTACHMENT_PASS assertions=%u fake_face_io=unavailable camera_calls=0\n",checks);
}
