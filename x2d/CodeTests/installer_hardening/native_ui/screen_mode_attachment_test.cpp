#define main factory_unit_main
#include "screen_factory_test.cpp"
#undef main
#undef C
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"SCREEN_MODE_ATTACHMENT_FAIL line=%d expr=%s\n",__LINE__,#v);exit(1);}}while(0)
#include "screen_mode_attachment.hpp"
#include "qt_abi_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_key_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQml/qqml.h>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtQuick/private/qquickpointhandler_p.h>
class IdleCamera:public QObject {Q_OBJECT
 Q_PROPERTY(bool lens_ring_rotation MEMBER ring NOTIFY lens_ring_rotationChanged)
public: bool ring=false;
Q_SIGNALS: void lens_ring_rotationChanged(bool value);void focus_pointChanged(uint value);
};
class IdleSystem:public QObject {Q_OBJECT Q_PROPERTY(bool media_processing MEMBER processing)
public:bool processing=false;
};
class IdleStorage:public QObject {Q_OBJECT Q_PROPERTY(int storage_status MEMBER status)
public:int status=0;
};
static int activeScreen=1;static unsigned char forwarded=0;
static void modeCall(void*o,int mode,int index,void**a){if(o==&camera&&index==20){if(mode==1)*static_cast<unsigned char*>(a[0])=forwarded;else{++writes;forwarded=*static_cast<unsigned char*>(a[0]);}}else if(o==&systemObject&&index==32){C(mode==1);*static_cast<int*>(a[0])=activeScreen;}else proxyCall(o,mode,index,a);}
int main(int argc,char**argv){QGuiApplication app(argc,argv);C(argc==3);C(QResource::registerResource(QString::fromLocal8Bit(argv[1])));C(QResource::registerResource(QString::fromLocal8Bit(argv[2])+"/extra.rcc"));QQmlEngine engine;QQuickWindow win;win.resize(800,600);QQmlEngine::setContextForObject(win.contentItem(),engine.rootContext());
 ScreenFactoryProfile p;void*cam=&camera,*sys=&systemObject;p.proxy.slots[0]=&cam;p.proxy.slots[1]=&sys;p.proxy.meta[0]=p.proxy.meta[1]=&QObject::staticMetaObject;p.proxy.cast=[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));};p.proxy.call[0]=p.proxy.call[1]=modeCall;p.proxy.fields[unsigned(FactoryField::EvfOnly)]={0,38,1,true,false,true};p.proxy.fields[unsigned(FactoryField::ScreensAllowed)]={1,33,4,false,false,true};p.proxy.fields[unsigned(FactoryField::ScreensActive)]={1,32,4,false,false,false};p.proxy.fields[unsigned(FactoryField::ForwardInput)]={0,20,1,false,false,true};p.backBit=1;p.evfBit=2;p.halfBit=2;
 ScreenKeyProfile kp;kp.key=qt_key_profile();kp.attached_object=[](void*o,void*(*f)(void*),bool c)->void*{return qmlAttachedPropertiesObject(static_cast<QObject*>(o),reinterpret_cast<QQmlAttachedPropertiesFunc>(f),c);};kp.keys_call=reinterpret_cast<AbiMetaCall>(QQuickKeysAttached::staticMetaObject.d.static_metacall);kp.window=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->window();};kp.active=[](const void*o)->void*{return static_cast<const QQuickWindow*>(o)->activeFocusItem();};kp.parent_item=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->parentItem();};KeyMapProfile km;km.instance=&cam;km.meta=&QObject::staticMetaObject;km.cast=p.proxy.cast;km.is_fn=[](const void*,int key,unsigned,int fn){return key==Qt::Key_A&&fn==11;};ScreenLiveProfile live{[](const void*m,const char*s){return static_cast<const QMetaObject*>(m)->indexOfMethod(s);},[](void*o,int call,int index,void**a){return QMetaObject::metacall(static_cast<QObject*>(o),QMetaObject::Call(call),index,a);}};
 QQmlComponent component(&engine);component.setData(R"QML(import QtQuick
 Item { objectName:"StateMachineViewModel_root";width:800;height:600
 property bool screenEVF:true
 property bool isShutdown:false
 property bool isMaintenanceUpgrade:false
 property bool isMaintenanceManual:false
 property bool exposing:false
 property int liveviewState:1
 property int exposureStatus:0
 property bool inSession:false
 property bool activeError:false
 property bool isUpgrading:false
 property bool isFormatting:false
 property bool isTethered:false
 property int controlRequests:0
 property bool argumentValid:false
 signal focusModeChanged()
 signal keyPressed(int key)
 signal customButton(int button,bool pressed)
 signal customWheel(int button,bool incremented)
 signal toggle_screen(string view,var arg)
 onToggle_screen:(view,arg)=>{if(view==="control_screen"){controlRequests++;argumentValid=typeof arg==="object"}}
 property int keyOverride:1
 property int requests:0
 property bool received:false
 signal setLiveView(bool enabled)
 onSetLiveView:(enabled)=>{requests++;received=enabled}
 })QML",QUrl("qrc:/screen-mode-fixture.qml"));C(component.isReady());auto*state=qobject_cast<QQuickItem*>(component.create());C(state);state->setParentItem(win.contentItem());win.show();state->forceActiveFocus();QCoreApplication::processEvents();auto abi=qt_abi_profile();auto api=abi_view_api(&abi);ScreenModeAttachment mode(api,qt_stock_profile(),qt_menu_profile(),qt_signal_profile(),p,kp,live,km,u"Live View in EVF Only");uint64_t now=0;auto tick=[&](){int rc=mode.tick(win.contentItem(),now+=50,800,600);if(rc)fprintf(stderr,"mode rc=%d\n",rc);return rc;};C(tick()==0&&!mode.intercepting());evf=true;screens=3;C(tick()==0&&forwarded==3&&!mode.intercepting());C(tick()==0&&mode.intercepting());
 QKeyEvent down(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier);QCoreApplication::sendEvent(&win,&down);C(down.isAccepted());C(tick()==0);auto*root=win.contentItem()->findChild<QQuickItem*>("X2dNativeScreenMode");C(root&&root->childItems().size()==1);{char16_t translated[]=u"新的取景提示";C(mode.set_notice(translated));translated[0]=u'坏';C(tick()==0);C(root->childItems()[0]->property("text").toString()==QString::fromUtf16(u"新的取景提示"));C(!mode.set_notice(nullptr));C(!mode.set_notice(u""));char16_t oversized[513];for(int i=0;i<512;i++)oversized[i]=u'X';oversized[512]=0;C(!mode.set_notice(oversized));C(tick()==0&&root->childItems()[0]->property("text").toString()==QString::fromUtf16(u"新的取景提示"));}C(root->childItems()[0]->isVisible());now+=1100;C(tick()==0&&!root->childItems()[0]->isVisible());QKeyEvent up(QEvent::KeyRelease,Qt::Key_A,Qt::NoModifier);QCoreApplication::sendEvent(&win,&up);C(up.isAccepted());
 activeScreen=2;C(tick()==0&&!mode.intercepting()&&forwarded==1);C(state->property("requests").toInt()==1&&state->property("received").toBool());for(int i=0;i<50;i++)C(tick()==0);C(state->property("requests").toInt()==1);activeScreen=1;C(tick()==0&&forwarded==3);C(tick()==0&&mode.intercepting());state->setProperty("exposing",true);activeScreen=2;C(tick()==0&&state->property("requests").toInt()==1);state->setProperty("exposing",false);C(tick()==0&&state->property("requests").toInt()==2);
 activeScreen=1;C(tick()==0);C(tick()==0&&mode.intercepting());mode.close();C(forwarded==1);C(!win.contentItem()->findChild<QQuickItem*>("X2dNativeScreenMode"));
 {ScreenModeAttachment synchronous(api,qt_stock_profile(),qt_menu_profile(),qt_signal_profile(),p,kp,live,km,u"Notice");auto*watch=signal_watch(qt_signal_profile(),state,state,"setLiveView(bool)",[](void*p){static_cast<ScreenModeAttachment*>(p)->close();},&synchronous,[](void*){});C(watch);activeScreen=2;unsigned before=writes;C(synchronous.tick(win.contentItem(),now+=50,800,600)==0);C(writes==before&&!synchronous.intercepting());C(!win.contentItem()->findChild<QQuickItem*>("X2dNativeScreenMode"));C(synchronous.tick(win.contentItem(),now+=50,800,600)==1);signal_unwatch(watch);}
 {ScreenModeAttachment destroyed(api,qt_stock_profile(),qt_menu_profile(),qt_signal_profile(),p,kp,live,km,u"Notice");activeScreen=1;C(destroyed.tick(win.contentItem(),now+=50,800,600)==0);C(destroyed.tick(win.contentItem(),now+=50,800,600)==0&&destroyed.intercepting());auto*own=win.contentItem()->findChild<QQuickItem*>("X2dNativeScreenMode");C(own);delete own;C(destroyed.tick(win.contentItem(),now+=50,800,600)!=0);C(forwarded==1&&!destroyed.intercepting());C(destroyed.tick(win.contentItem(),now+=50,800,600)==1);}
 {ScreenModeAttachment freshClose(api,qt_stock_profile(),qt_menu_profile(),qt_signal_profile(),p,kp,live,km,u"Notice");activeScreen=1;C(freshClose.tick(win.contentItem(),now+=50,800,600)==0);C(freshClose.tick(win.contentItem(),now+=50,800,600)==0);state->setProperty("keyOverride",4);freshClose.close();C(forwarded==4);}
 {IdleCamera c;IdleSystem system;IdleStorage storage;void*cp=&c,*sp=&system,*st=&storage;auto timerProfile=p;timerProfile.proxy.slots[0]=&cp;timerProfile.proxy.slots[1]=&sp;timerProfile.proxy.slots[2]=&st;timerProfile.proxy.meta[2]=&QObject::staticMetaObject;
 timerProfile.proxy.call[0]=[](void*,int mode,int index,void**a){modeCall(&camera,mode,index,a);};timerProfile.proxy.call[1]=[](void*,int mode,int index,void**a){modeCall(&systemObject,mode,index,a);};
 auto lp=live;lp.point_size=sizeof(QQuickPointHandler);C(lp.point_size==32);lp.point_construct=[](void*at,void*parent){new(at)QQuickPointHandler(static_cast<QQuickItem*>(parent));};lp.point_complete=[](void*p){static_cast<QQmlParserStatus*>(static_cast<QQuickPointHandler*>(p))->componentComplete();};lp.point_property=reinterpret_cast<AbiMetaCall>(QQuickPointerHandler::staticMetaObject.d.static_metacall);lp.map_type=QMetaType::fromType<QVariantMap>().iface();lp.liveview_active=1;
 QQmlComponent dc(&engine);dc.setData("import QtQuick; Item {property string mainState: \"liveview\"}",QUrl("qrc:/timeout-drawer.qml"));C(dc.isReady());auto*drawer=dc.create();
 ScreenModeAttachment timer(api,qt_stock_profile(),qt_menu_profile(),qt_signal_profile(),timerProfile,kp,lp,km,u"Notice");activeScreen=1;evf=false;timer.configure_timeout(10,drawer,true);uint64_t t=100000;
 auto run=[&](uint64_t delta){t+=delta;C(timer.tick(win.contentItem(),t,800,600)==0);fprintf(stderr,"TIMEOUT t=%llu hooks=%u wait=%u calls=%u stateCalls=%d arg=%u\n",(unsigned long long)t,timer.timeout_hooks_ready(),timer.timeout_waiting(),timer.timeout_requests(),state->property("controlRequests").toInt(),state->property("argumentValid").toBool());};run(0);C(timer.timeout_requests()==0);run(9999);C(timer.timeout_requests()==0);run(1);C(timer.timeout_requests()==1&&state->property("controlRequests").toInt()==1&&state->property("argumentValid").toBool());run(20000);C(timer.timeout_requests()==1);
 C(QMetaObject::invokeMethod(state,"keyPressed",Q_ARG(int,1)));run(0);run(9999);C(timer.timeout_requests()==1);run(1);C(timer.timeout_requests()==2);
 timer.configure_timeout(10,drawer,false);run(20000);timer.configure_timeout(10,drawer,true);run(0);run(10000);C(timer.timeout_requests()==3);
 state->setProperty("exposureStatus",16);run(20000);state->setProperty("exposureStatus",0);run(0);run(10000);C(timer.timeout_requests()==4);
 auto*point=win.contentItem()->findChild<QQuickPointHandler*>();C(point);QMouseEvent press(QEvent::MouseButtonPress,QPointF(200,200),QPointF(200,200),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(&win,&press);QCoreApplication::processEvents();C(point->active());run(20000);C(timer.timeout_requests()==4);
 QMouseEvent release(QEvent::MouseButtonRelease,QPointF(200,200),QPointF(200,200),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(&win,&release);QCoreApplication::processEvents();C(!point->active());run(0);run(10000);C(timer.timeout_requests()==5);
 timer.close();C(!win.contentItem()->findChild<QQuickPointHandler*>());delete drawer;
 }
 delete state;printf("SCREEN_MODE_ATTACHMENT_PASS assertions=%u actual_window_key_forwarding=1 actual_qml_signal_bool=1 hardware_calls=0\n",checks);
}
#include "screen_mode_attachment_test.moc"
