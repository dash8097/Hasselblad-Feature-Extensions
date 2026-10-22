#include "face_popup_attachment.hpp"
#include "qt_stock_profile.hpp"
#include "qt_abi_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_binding_profile.hpp"
#include "qt_menu_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlListReference>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QMetaObject>
#include <QThread>
#include <QKeyEvent>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQml/qqml.h>
#include "qt_key_profile.hpp"
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned checks,writes;static uint8_t modeValue=0;static bool master=false,ack=true;static uint32_t options=0;static QObject camera,systemObject;static FacePopupAttachment*active=nullptr;static bool closeOnWrite=false;
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"FACE_ATTACH_FAIL line=%d expression=%s\n",__LINE__,#v);exit(1);}}while(0)
static void call(void*o,int op,int index,void**a){C(op==1||op==2);if(op==2){++writes;if(closeOnWrite&&active)active->close();if(!ack)return;}if(o==&camera){C(index==68);if(op==1)*static_cast<uint8_t*>(a[0])=modeValue;else modeValue=*static_cast<uint8_t*>(a[0]);}else{C(o==&systemObject);if(index==12){if(op==1)*static_cast<bool*>(a[0])=master;else master=*static_cast<bool*>(a[0]);}else{C(index==13);if(op==1)*static_cast<uint32_t*>(a[0])=options;else options=*static_cast<uint32_t*>(a[0]);}}}
static FaceFactoryProfile factory(void**cam,void**sys){FaceFactoryProfile p;p.proxy.slots[0]=cam;p.proxy.slots[1]=sys;p.proxy.meta[0]=p.proxy.meta[1]=&QObject::staticMetaObject;p.proxy.cast=[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));};p.proxy.call[0]=p.proxy.call[1]=call;p.face={0,68,1,false,false,true};p.master={1,12,1,true,false,true};p.options={1,13,4,false,false,true};p.modes={0,2,1};p.list_construct=[](void*p,const void*v){new(p)QQmlListReference(*static_cast<const QVariant*>(v));};p.list_destroy=[](void*p){static_cast<QQmlListReference*>(p)->~QQmlListReference();};p.list_count=[](const void*p){return std::ptrdiff_t(static_cast<const QQmlListReference*>(p)->count());};p.list_at=[](const void*p,std::ptrdiff_t i)->void*{return static_cast<const QQmlListReference*>(p)->at(i);};p.method_index=[](const void*m,const char*n){return static_cast<const QMetaObject*>(m)->indexOfMethod(n);};p.metacall=[](void*o,int c,int i,void**a){return QMetaObject::metacall(static_cast<QObject*>(o),QMetaObject::Call(c),i,a);};return p;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);C(argc==3);C(QResource::registerResource(QString::fromLocal8Bit(argv[1])));C(QResource::registerResource(QString::fromLocal8Bit(argv[2])+"/extra.rcc"));QQmlEngine engine;QQuickWindow win;win.resize(1280,960);QQmlEngine::setContextForObject(win.contentItem(),engine.rootContext());auto abi=qt_abi_profile();auto stock=qt_stock_profile();auto api=abi_view_api(&abi);auto sig=qt_signal_profile();auto binding=qt_binding_profile();auto menu=qt_menu_profile();void*cam=&camera,*sys=&systemObject;auto f=factory(&cam,&sys);FacePopupProfile p{QMetaType::fromType<QQmlListProperty<QQuickItem>>().iface()};
 auto scene=[&](){QQmlComponent c(&engine);c.setData(R"(import QtQuick
Item {width:1280;height:960
 Item {id:l;objectName:"ControlScreen_popupLoader";property Item item:original;property int closedCount:0
 Item {id:original;objectName:"PopoverFaceDetection_root";width:1280;height:960;visible:true;enabled:true
 property string heading:"人脸检测";property int faceDetection:0;property bool canChangeFaceDetection:true
 property list<QtObject> faceDetectionModel:[QtObject{property int mode:2;property bool canSelect:true},QtObject{property int mode:0;property bool canSelect:true}]
 signal closePopup();onClosePopup:{l.closedCount++;l.item=null;visible=false}
 }
 }
})",QUrl());if(c.isError())fprintf(stderr,"%s\n",qPrintable(c.errorString()));auto*o=qobject_cast<QQuickItem*>(c.create(engine.rootContext()));C(o);o->setParent(win.contentItem());o->setParentItem(win.contentItem());return o;};
 auto make=[&](){auto*s=scene();active=new FacePopupAttachment(api,stock,menu,binding,sig,f,p,{});C(active->tick(s,0,1280,960)==0&&active->attached());return s;};auto*sceneRoot=make();auto*loader=sceneRoot->findChild<QObject*>("ControlScreen_popupLoader");auto*orig=sceneRoot->findChild<QQuickItem*>("PopoverFaceDetection_root");C(!orig->isVisible()&&!orig->isEnabled());C(static_cast<QQuickItem*>(sceneRoot)->childItems().size()>=1);C(active->key(FacePopupKey::Previous));C(active->key(FacePopupKey::Accept)&&master&&writes==1);C(active->attached());C(active->tick(sceneRoot,100,1280,960)==0&&options==1&&writes==2);C(active->tick(sceneRoot,200,1280,960)==0&&modeValue==2&&writes==3);C(active->tick(sceneRoot,300,1280,960)==0&&!active->attached());C(loader->property("closedCount").toInt()==1);C(orig->isEnabled());
 // Same global controller keeps ownership of debug master across popup instances.
 C(active->tick(sceneRoot,325,1280,960)==0);loader->setProperty("item",QVariant::fromValue<QObject*>(orig));orig->setProperty("visible",true);C(active->tick(sceneRoot,350,1280,960)==0&&active->attached());C(active->key(FacePopupKey::Previous));C(active->key(FacePopupKey::Accept)&&options==0);C(active->tick(sceneRoot,450,1280,960)==0&&!master);C(active->tick(sceneRoot,550,1280,960)==0&&modeValue==2);C(active->tick(sceneRoot,650,1280,960)==0&&!active->attached());active->close();delete active;active=nullptr;delete sceneRoot;
 // Current Loader ownership is checked at click time, before the next timer tick.
 sceneRoot=make();loader=sceneRoot->findChild<QObject*>("ControlScreen_popupLoader");auto staleWrites=writes;C(loader->setProperty("item",QVariant::fromValue<QQuickItem*>(nullptr)));C(!stock_get_object(stock,loader,"item"));C(active->key(FacePopupKey::Previous));C(active->key(FacePopupKey::Accept));C(writes==staleWrites);active->close();delete active;active=nullptr;delete sceneRoot;
 // A live but visually detached old Loader is also no longer the current route.
 sceneRoot=make();loader=sceneRoot->findChild<QObject*>("ControlScreen_popupLoader");staleWrites=writes;static_cast<QQuickItem*>(loader)->setParentItem(nullptr);C(active->key(FacePopupKey::Previous));C(active->key(FacePopupKey::Accept));C(writes==staleWrites);active->close();delete active;active=nullptr;delete sceneRoot;
 // Loader survives and still names a detached item: it is not an actionable route.
 sceneRoot=make();orig=sceneRoot->findChild<QQuickItem*>("PopoverFaceDetection_root");staleWrites=writes;orig->setParentItem(nullptr);C(active->key(FacePopupKey::Previous));C(active->key(FacePopupKey::Accept));C(writes==staleWrites);active->close();delete active;active=nullptr;delete sceneRoot;
 // Unknown setter is sent once; 31 subsequent reads end in visible error.
 modeValue=0;master=false;options=0;ack=false;auto before=writes;sceneRoot=make();C(active->key(FacePopupKey::Previous)&&active->key(FacePopupKey::Accept));for(unsigned i=1;i<=32;i++)C(active->tick(sceneRoot,i*100,1280,960)==0);C(writes==before+1&&active->attached());C(active->key(FacePopupKey::Abort));C(active->tick(sceneRoot,3300,1280,960)==0&&!active->attached());active->close();delete active;active=nullptr;delete sceneRoot;ack=true;
 // Explicit close during synchronous proxy callback defers destruction safely.
 sceneRoot=make();closeOnWrite=true;C(active->key(FacePopupKey::Previous));active->key(FacePopupKey::Accept);C(!active->attached()&&!active->dispatching());delete active;active=nullptr;closeOnWrite=false;delete sceneRoot;
 // Root deletion and proxy disappearance never dispatch a setter.
 modeValue=0;master=false;options=0;sceneRoot=make();before=writes;delete sceneRoot;active->close();C(writes==before);delete active;active=nullptr;
 sceneRoot=scene();cam=nullptr;active=new FacePopupAttachment(api,stock,menu,binding,sig,f,p,{});C(active->tick(sceneRoot,0,1280,960)!=0&&!active->attached());C(writes==before);active->close();delete active;active=nullptr;delete sceneRoot;cam=&camera;
 // Foreign visible/enabled overrides are not overwritten or fought each tick.
 for(int field=0;field<2;field++){modeValue=0;master=false;options=0;sceneRoot=make();orig=sceneRoot->findChild<QQuickItem*>("PopoverFaceDetection_root");orig->setProperty(field?"enabled":"visible",true);C(active->tick(sceneRoot,50,1280,960)!=0&&!active->attached());C(orig->property(field?"enabled":"visible").toBool());before=writes;for(int i=1;i<10;i++)active->tick(sceneRoot,50+i*50,1280,960);C(writes==before&&!active->attached());active->close();delete active;active=nullptr;delete sceneRoot;}
 // Physical key arrives through the real existing Qt Keys.forwardTo chain.
 modeValue=0;master=false;options=0;sceneRoot=scene();active=new FacePopupAttachment(api,stock,menu,binding,sig,f,p,{});ScreenKeyProfile kp;kp.key=qt_key_profile();kp.attached_object=[](void*o,void*(*f)(void*),bool c)->void*{return qmlAttachedPropertiesObject(static_cast<QObject*>(o),reinterpret_cast<QQmlAttachedPropertiesFunc>(f),c);};kp.keys_call=reinterpret_cast<AbiMetaCall>(QQuickKeysAttached::staticMetaObject.d.static_metacall);kp.window=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->window();};kp.active=[](const void*o)->void*{return static_cast<const QQuickWindow*>(o)->activeFocusItem();};kp.parent_item=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->parentItem();};KeyMapProfile km{};km.instance=&cam;km.meta=&QObject::staticMetaObject;km.cast=f.proxy.cast;km.is_fn=[](const void*,int k,unsigned,int fn){return (k==Qt::Key_A&&fn==27)||(k==Qt::Key_B&&fn==28)||(k==Qt::Key_C&&fn==6);};C(active->configure_keys(kp,km));C(active->tick(sceneRoot,0,1280,960)==0);win.show();auto*popup=sceneRoot->findChild<QQuickItem*>("X2dNativeFacePopup");C(popup&&popup->property("keyOverride").toInt()==2);popup->forceActiveFocus();QCoreApplication::processEvents();C(active->tick(sceneRoot,50,1280,960)==0);QKeyEvent up(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier);QCoreApplication::sendEvent(&win,&up);C(up.isAccepted());QKeyEvent accept(QEvent::KeyPress,Qt::Key_B,Qt::NoModifier);before=writes;QCoreApplication::sendEvent(&win,&accept);fprintf(stderr,"KEY_RESULT accepted=%d writes=%u before=%u master=%d\n",accept.isAccepted(),writes,before,master);C(accept.isAccepted()&&writes==before+1&&master);active->close();delete active;active=nullptr;delete sceneRoot;
 // Full live-view trees exceed 512 objects; a late Loader under the same root
 // still needs discovery after the initial 20-second search window.
 modeValue=0;master=false;options=0;sceneRoot=scene();loader=sceneRoot->findChild<QObject*>("ControlScreen_popupLoader");C(loader);loader->setObjectName("NotYetAvailable");
 for(unsigned i=0;i<2500;i++)new QQuickItem(sceneRoot);
 active=new FacePopupAttachment(api,stock,menu,binding,sig,f,p,{});
 for(unsigned i=0;i<100;i++)C(active->tick(sceneRoot,i*250,1280,960)==0&&!active->has_source());
 loader->setObjectName("ControlScreen_popupLoader");C(active->tick(sceneRoot,26000,1280,960)==0&&active->attached());
 auto*duplicate=new QQuickItem(sceneRoot);duplicate->setObjectName("ControlScreen_popupLoader");before=writes;active->key(FacePopupKey::Previous);active->key(FacePopupKey::Accept);C(writes==before);active->close();delete active;active=nullptr;delete sceneRoot;
 printf("FACE_ATTACHMENT_PASS assertions=%u hardware_calls=0\n",checks);
}
