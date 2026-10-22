#define main factory_unit_main
#include "screen_factory_test.cpp"
#undef main
#undef C
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"SCREEN_ATTACHMENT_FAIL line=%d expr=%s\n",__LINE__,#v);exit(1);}}while(0)
#include "screen_attachment.hpp"
#include "qt_abi_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_binding_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_key_profile.hpp"
#include <QtQuick/private/qquickitem_p.h>
#include <QtQml/qqml.h>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QQuickImageProvider>
#include <QFile>
#include <QImage>
static QList<QQuickItem*> visual(QQuickItem*r,const QString&n){QList<QQuickItem*> out;for(auto*c:r->childItems()){if(c->objectName()==n)out.append(c);out.append(visual(c,n));}return out;}
class Icons:public QQuickImageProvider{public:Icons():QQuickImageProvider(Image){}QImage requestImage(const QString&id,QSize*s,const QSize&)override{QImage im(":/icons/"+id+".svg");*s=im.size();return im;}};
static const char*qml=R"QML(import QtQuick
Item {
 id:root; objectName:"ControlDrawer_root";width:800;height:600
 property string mainState:"main_menu"
 property bool originalVisible:true
 property bool originalEnabled:true
 property int currentIndex:0
 Repeater {
  model:3
  FocusScope {
   // 4.2.0 HblLoader is a zero-width wrapper with a centered inner Loader.
   // Its loaded item's coordinates belong to the inner Loader.
   objectName:"specialItem";x:400;y:index*100+44
   property int rowIndex:index
   property alias item:inner.item
   Loader {
    id:inner;anchors.centerIn:parent;width:800;height:88
    property int rowIndex:parent.rowIndex
    sourceComponent:Component { Item {
    width:800;height:88;visible:root.originalVisible;enabled:root.originalEnabled
    property var list:root
    property int itemIndex:parent.rowIndex
    property QtObject model:QtObject {
     property string itemName:itemIndex==0?"screens_allowed":itemIndex==1?"screen_idle_timeout":"allow_variable_fps"
     property string text1:itemIndex==0?"Display":itemIndex==1?"Timeout":"Adaptive FPS"
     property string description:itemIndex==0?"Original display description":""
     property bool itemEnabled:true
     property int proxy:itemIndex==2?2:4
    }
    }}
   }
  }
 }
})QML";
int main(int argc,char**argv){QGuiApplication app(argc,argv);C(argc==3);C(QResource::registerResource(QString::fromLocal8Bit(argv[1])));C(QResource::registerResource(QString::fromLocal8Bit(argv[2])+"/extra.rcc"));QQmlEngine engine;engine.addImageProvider("svg",new Icons);QQuickWindow win;win.resize(800,600);QQmlEngine::setContextForObject(win.contentItem(),engine.rootContext());
 ScreenFactoryProfile p;void*cam=&camera,*sys=&systemObject,*fmt=&formatterObject;p.proxy.slots[0]=&cam;p.proxy.slots[1]=&sys;p.proxy.meta[0]=p.proxy.meta[1]=&QObject::staticMetaObject;p.proxy.cast=[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));};p.proxy.call[0]=p.proxy.call[1]=proxyCall;p.proxy.fields[unsigned(FactoryField::EvfOnly)]={0,38,1,true,false,true};p.proxy.fields[unsigned(FactoryField::ScreensAllowed)]={1,33,4,false,false,true};p.proxy.fields[unsigned(FactoryField::ScreenTimeout)]={1,30,4,false,true,true};p.formatter.slot=&fmt;p.formatter.meta=&QObject::staticMetaObject;p.formatter.cast=p.proxy.cast;p.formatter.call=formatterCall;auto stock=qt_stock_profile();p.formatter.string_ctor=stock.string_ctor;p.formatter.string_deallocate=stock.string_deallocate;p.list_destroy=[](void*x){static_cast<QStringList*>(x)->~QStringList();};p.backBit=1;p.evfBit=2;p.systemProxy=4;p.cameraProxy=2;p.halfBit=2;
 auto abi=qt_abi_profile();auto api=abi_view_api(&abi);ScreenControlStyle style{88,30,30,28,28,22,0,400,40,132,132,4.3};
 QQmlComponent component(&engine);component.setData(qml,QUrl("qrc:/screen-attachment-fixture.qml"));C(component.isReady());auto make=[&](){auto*r=qobject_cast<QQuickItem*>(component.create());C(r);r->setParentItem(win.contentItem());return r;};auto*root=make();
 // Real menu sessions grow beyond the old 512-item whole-scene limit.
 // Unrelated original pages and the active drawer are independently bounded;
 // feature-owned popup trees must never consume the original-tree budget.
 auto*unrelated=new QQuickItem(win.contentItem());unrelated->setObjectName("unrelatedOriginalPage");
 for(int i=0;i<800;i++)new QQuickItem(unrelated);
 for(int i=0;i<800;i++)new QQuickItem(root);
 auto*ownedTree=new QQuickItem(root);ownedTree->setObjectName("X2dNativeOtherFeature");
 for(int i=0;i<4500;i++)new QQuickItem(ownedTree);
 auto originals=[&](){QList<QQuickItem*> result;for(auto*l:visual(root,"specialItem")){auto*item=qobject_cast<QQuickItem*>(l->property("item").value<QObject*>());if(item)result.append(item);}C(result.size()==3);return result;};auto rows=originals();
 ScreenAttachment attach(api,stock,&abi,qt_menu_profile(),qt_binding_profile(),qt_signal_profile(),p,style);ScreenKeyProfile kp;kp.key=qt_key_profile();kp.attached_object=[](void*o,void*(*f)(void*),bool c)->void*{return qmlAttachedPropertiesObject(static_cast<QObject*>(o),reinterpret_cast<QQmlAttachedPropertiesFunc>(f),c);};kp.keys_call=reinterpret_cast<AbiMetaCall>(QQuickKeysAttached::staticMetaObject.d.static_metacall);kp.window=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->window();};kp.active=[](const void*o)->void*{return static_cast<const QQuickWindow*>(o)->activeFocusItem();};kp.parent_item=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->parentItem();};KeyMapProfile km;km.instance=&cam;km.meta=&QObject::staticMetaObject;km.cast=p.proxy.cast;km.is_fn=[](const void*,int key,unsigned,int fn){return (key==Qt::Key_Return&&fn==8)||(key==Qt::Key_Down&&fn==26);};C(attach.configure_keys(kp,km));win.show();root->forceActiveFocus();QCoreApplication::processEvents();uint64_t now=0;auto tick=[&](bool touch=true){int rc=attach.tick(win.contentItem(),now+=50,800,600,touch);if(rc)fprintf(stderr,"attachment rc=%d attached=%u scans=%u\n",rc,attach.attached(),attach.scans());return rc;};C(tick()==0&&attach.attached()==3);for(auto*r:rows)C(!r->isVisible()&&!r->isEnabled());C(visual(root,"X2dNativeScreenControl").size()==3);
 C(tick()==0);unsigned scanned=attach.scans();for(int i=0;i<100;i++)C(tick()==0);C(attach.scans()==scanned);
 for(auto*r:rows){auto native=visual(r->parentItem(),"X2dNativeScreenControl");C(native.size()==1);C(native[0]->parentItem()==r->parentItem());C(native[0]->mapToScene(QPointF())==r->mapToScene(QPointF()));C(native[0]->mapToScene(QPointF()).x()==0);C(native[0]->mapToScene(QPointF(native[0]->width(),0)).x()==800);}
 // The real FPS delegate starts with no description and exactly one row.
 C(rows[2]->height()>88);auto fpsControls=visual(rows[2]->parentItem(),"X2dNativeScreenControl");C(fpsControls.size()==1);auto fpsDescriptions=visual(fpsControls[0],"X2dNativeScreenDescription");C(fpsDescriptions.size()==1&&fpsDescriptions[0]->isVisible());C(fpsDescriptions[0]->height()>=fpsDescriptions[0]->implicitHeight());
 // Reconciliation retains subscriptions on surviving nodes.
 unsigned connections=attach.subscriptionConnections();
 C(QMetaObject::invokeMethod(root,"childrenChanged",Qt::DirectConnection));C(tick()==0);C(attach.subscriptionConnections()==connections);
 for(int i=0;i<20;i++){auto*extra=new QQuickItem(root);extra->setObjectName("temporaryFactoryItem");C(tick()==0);unsigned added=attach.subscriptionConnections();C(added>connections);C(QMetaObject::invokeMethod(root,"childrenChanged",Qt::DirectConnection));C(tick()==0);C(attach.subscriptionConnections()==added);delete extra;C(tick()==0);C(attach.subscriptionConnections()==added);connections=added;}
 root->setProperty("currentIndex",1);C(tick(false)==0);C(attach.key(ScreenControlKey::Open));C(attach.key(ScreenControlKey::Down));C(attach.key(ScreenControlKey::Accept));C(timeout==0);C(tick(false)==0);
 root->setProperty("currentIndex",2);C(tick(false)==0);QKeyEvent keyDown(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QCoreApplication::sendEvent(&win,&keyDown);C(keyDown.isAccepted()&&fpsValue);C(tick(false)==0);root->setProperty("currentIndex",0);C(tick()==0);C(attach.key(ScreenControlKey::Open));C(attach.key(ScreenControlKey::Down));C(attach.key(ScreenControlKey::Down));C(attach.key(ScreenControlKey::Accept));C(tick()==0);C(tick()==0);
 root->setProperty("mainState","live_view");C(tick()==0&&attach.attached()==0);for(auto*r:rows)C(r->isVisible()&&r->isEnabled());C(rows[2]->height()==88);root->setProperty("originalVisible",false);root->setProperty("originalEnabled",false);for(auto*r:rows)C(!r->isVisible()&&!r->isEnabled());root->setProperty("originalVisible",true);root->setProperty("originalEnabled",true);root->setProperty("mainState","main_menu");C(tick()==0&&attach.attached()==3);
 // 原始控件被外部删除：销毁信号撤销borrow，不恢复到已释放对象。
 delete root;root=make();C(tick()==0&&attach.attached()==3);rows=originals();root->setProperty("currentIndex",2);auto*fm=rows[2]->property("model").value<QObject*>();C(fm);fm->setProperty("itemEnabled",false);unsigned old=writes;C(tick()==0);attach.key(ScreenControlKey::Open);C(writes==old);
 rows[0]->setX(30);rows[0]->setWidth(400);rows[0]->setHeight(130);C(tick()==0);auto owned=visual(rows[0]->parentItem(),"X2dNativeScreenControl");C(owned.size()==1&&owned[0]->x()==30&&owned[0]->width()==400&&owned[0]->height()==130);auto descriptions=visual(owned[0],"X2dNativeScreenDescription");C(descriptions.size()==1&&descriptions[0]->isVisible()&&descriptions[0]->property("text").toString()=="Original display description");for(auto*popup:visual(win.contentItem(),"X2dNativeScreenSelector"))C(popup->width()==800);
 // foreign visible变更不能被下一tick重新覆盖；退出菜单后才重新接受该原行。
 rows[0]->setVisible(true);C(tick()!=0);C(rows[0]->isVisible());tick();C(rows[0]->isVisible());root->setProperty("mainState","live_view");tick();root->setProperty("mainState","main_menu");C(tick()==0&&attach.attached()==3);
 struct Preferences{int seconds=0;bool confirmed=true;unsigned writes=0;} prefs;
 const char16_t*choices[]={u"关闭",u"10秒",u"20秒",u"30秒",u"1分钟",u"2分钟",u"5分钟"};
 C(attach.configure_liveview({&prefs,[](void*p,int&n,bool&confirmed){auto&s=*static_cast<Preferences*>(p);n=s.seconds;confirmed=s.confirmed;return true;},[](void*p,int n,uint64_t){auto&s=*static_cast<Preferences*>(p);s.seconds=n;s.confirmed=false;s.writes++;return true;}},u"取景超时",choices));
 root->setProperty("currentIndex",1);root->forceActiveFocus();C(tick()==0);C(rows[1]->height()==176);auto lv=visual(root,"X2dNativeLiveviewTimeout");C(lv.size()==1);auto sv=visual(rows[1]->parentItem(),"X2dNativeScreenControl");C(sv.size()==1);C(lv[0]->y()+88==sv[0]->y());C(lv[0]->height()==88&&sv[0]->height()==88);
 C(attach.key(ScreenControlKey::Open));C(attach.key(ScreenControlKey::Down));C(attach.key(ScreenControlKey::Accept));C(prefs.seconds==10&&prefs.writes==1);C(tick()==0);prefs.confirmed=true;C(tick()==0);
 QKeyEvent nextRow(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier);QCoreApplication::sendEvent(&win,&nextRow);C(nextRow.isAccepted());C(root->property("currentIndex").toInt()==1);C(tick()==0);C(attach.key(ScreenControlKey::Open));C(attach.key(ScreenControlKey::Up));C(attach.key(ScreenControlKey::Accept));C(prefs.writes==1);C(tick()==0);
 root->setProperty("mainState","liveview");C(tick()==0);C(rows[1]->height()==88);C(visual(root,"X2dNativeLiveviewTimeout").isEmpty());root->setProperty("mainState","main_menu");C(tick()==0);C(rows[1]->height()==176);
 attach.close();C(attach.attached()==0);for(auto*r:rows)C(r->isVisible()&&r->isEnabled());C(rows[1]->height()==88);C(!attach.key(ScreenControlKey::Open));delete root;
 printf("SCREEN_ATTACHMENT_PASS assertions=%u stable_tick_scans=0 actual_qt_original_widgets=1 hardware_calls=0\n",checks);
}
