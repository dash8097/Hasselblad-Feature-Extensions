#include "focus_attachment.hpp"
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

static const void*countRoot;static unsigned rootScans;static AbiArray(*realChildren)(const void*);static AbiArray countedChildren(const void*p){if(p==countRoot)++rootScans;return realChildren(p);}
static unsigned checks,writes;static int language=8;static QObject systemObject;static FocusAttachment*active=nullptr;static bool closeOnWrite=false;static QObject*lastSender;
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"FOCUS_ATTACH_FAIL line=%d expression=%s\n",__LINE__,#v);exit(1);}}while(0)
static FocusFactoryProfile factory(void**sys){FocusFactoryProfile f;auto&p=f.common;p.proxy.slots[1]=sys;p.proxy.meta[1]=&QObject::staticMetaObject;p.proxy.cast=[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));};p.proxy.call[1]=[](void*o,int op,int i,void**a){C(o==&systemObject&&op==1&&i==20);*static_cast<uint16_t*>(a[0])=uint16_t(language);};p.list_construct=[](void*p,const void*v){new(p)QQmlListReference(*static_cast<const QVariant*>(v));};p.list_destroy=[](void*p){static_cast<QQmlListReference*>(p)->~QQmlListReference();};p.list_count=[](const void*p){return std::ptrdiff_t(static_cast<const QQmlListReference*>(p)->count());};p.list_at=[](const void*p,std::ptrdiff_t i)->void*{return static_cast<const QQmlListReference*>(p)->at(i);};p.method_index=[](const void*m,const char*n){return static_cast<const QMetaObject*>(m)->indexOfMethod(n);};p.metacall=[](void*o,int c,int i,void**a){C(c==0);++writes;lastSender=static_cast<QObject*>(o);if(closeOnWrite&&active)active->close();return QMetaObject::metacall(static_cast<QObject*>(o),QMetaObject::Call(c),i,a);};return f;}
int main(int argc,char**argv){setbuf(stderr,nullptr);QGuiApplication app(argc,argv);C(argc==3);C(QResource::registerResource(QString::fromLocal8Bit(argv[1])));C(QResource::registerResource(QString::fromLocal8Bit(argv[2])+"/extra.rcc"));QQmlEngine engine;QQuickWindow win;win.resize(1280,960);QQmlEngine::setContextForObject(win.contentItem(),engine.rootContext());auto abi=qt_abi_profile();auto stock=qt_stock_profile();auto api=abi_view_api(&abi);auto sig=qt_signal_profile();auto binding=qt_binding_profile();auto menu=qt_menu_profile();realChildren=menu.children;menu.children=countedChildren;void*sys=&systemObject;auto f=factory(&sys);FacePopupProfile p{QMetaType::fromType<QQmlListProperty<QQuickItem>>().iface()};
 auto scene=[&](){QQmlComponent c(&engine);c.setData(R"(import QtQuick
Item {width:1280;height:960
 Item {id:control;objectName:"ControlScreen_focusScope";width:1280;height:960
 property QtObject viewModel:QtObject{id:model;objectName:"FocusModelFixture";property string mainState:"focus_mode";property bool focusModeSelectable:true}
 Item {id:l;objectName:"ControlScreen_popupLoader";property Item item:original;signal loaded()
 Item {id:original;objectName:"PopoverFocusMode_root";width:1280;height:960;property bool inheritedVisible:true;visible:inheritedVisible;enabled:true
 property string heading:"对焦模式";property int focusMode:1;property bool acknowledge:true;property int sends:0
 property list<QtObject> focusModeModel:[QtObject{property int focusMode:1;property bool valid:true},QtObject{property int focusMode:3;property bool valid:true}]
 signal setFocusMode(int mode);onSetFocusMode:(mode)=>{sends++;if(acknowledge)focusMode=mode}
 }
 }
 }
})",QUrl());if(c.isError())fprintf(stderr,"%s\n",qPrintable(c.errorString()));auto*o=qobject_cast<QQuickItem*>(c.create(engine.rootContext()));C(o);o->setParent(win.contentItem());o->setParentItem(win.contentItem());return o;};
 auto make=[&](){auto*s=scene();active=new FocusAttachment(api,stock,menu,binding,sig,f,p,{});C(active->tick(s,100,1280,960)==0&&active->attached());return s;};
 auto*root=make();auto*orig=root->findChild<QQuickItem*>("PopoverFocusMode_root");auto*model=root->findChild<QObject*>("FocusModelFixture");C(model);C(!orig->isVisible()&&!orig->isEnabled());C(active->key(FocusKey::Next)&&active->key(FocusKey::Accept));C(writes==1&&orig->property("focusMode").toInt()==2);C(active->tick(root,200,1280,960)==0&&!active->attached());C(model->property("mainState").toString().isEmpty());C(orig->isVisible()&&orig->isEnabled());orig->setProperty("inheritedVisible",false);C(!orig->isVisible());active->close();delete active;active=nullptr;delete root;
 root=make();orig=root->findChild<QQuickItem*>("PopoverFocusMode_root");orig->setProperty("acknowledge",false);auto before=writes;C(active->key(FocusKey::Next)&&active->key(FocusKey::Accept));for(int i=1;i<=31;i++)C(active->tick(root,100+i*100,1280,960)==0);C(writes==before+1&&active->attached());auto*status=root->findChild<QObject*>("X2dFocusStatus");C(status&&status->property("text").toString()==QString::fromUtf16(focus_text(language,4)));C(active->key(FocusKey::Abort));C(active->tick(root,3300,1280,960)==0&&!active->attached());C(writes==before+1);active->close();delete active;active=nullptr;delete root;
 root=make();closeOnWrite=true;C(active->key(FocusKey::Next));C(active->key(FocusKey::Accept));C(!active->attached()&&!active->dispatching());delete active;active=nullptr;closeOnWrite=false;delete root;
 for(int mode=0;mode<4;mode++){root=make();before=writes;if(mode==0){delete root;root=nullptr;}if(mode==1)delete root->findChild<QObject*>("FocusModelFixture");if(mode==2)delete root->findChild<QObject*>("ControlScreen_popupLoader");if(mode==3)delete root->findChild<QObject*>("PopoverFocusMode_root");if(root)active->tick(root,200,1280,960);active->close();C(writes==before&&!active->attached());delete active;active=nullptr;delete root;}
 for(int field=0;field<2;field++){root=make();orig=root->findChild<QQuickItem*>("PopoverFocusMode_root");orig->setProperty(field?"enabled":"visible",true);C(active->tick(root,200,1280,960)!=0&&!active->attached());C(orig->property(field?"enabled":"visible").toBool());before=writes;for(int i=1;i<8;i++)active->tick(root,200+i*100,1280,960);C(!active->attached()&&writes==before);active->close();delete active;active=nullptr;delete root;}
 root=scene();active=new FocusAttachment(api,stock,menu,binding,sig,f,p,{});ScreenKeyProfile kp;kp.key=qt_key_profile();kp.attached_object=[](void*o,void*(*f)(void*),bool c)->void*{return qmlAttachedPropertiesObject(static_cast<QObject*>(o),reinterpret_cast<QQmlAttachedPropertiesFunc>(f),c);};kp.keys_call=reinterpret_cast<AbiMetaCall>(QQuickKeysAttached::staticMetaObject.d.static_metacall);kp.window=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->window();};kp.active=[](const void*o)->void*{return static_cast<const QQuickWindow*>(o)->activeFocusItem();};kp.parent_item=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->parentItem();};KeyMapProfile km{};km.instance=&sys;km.meta=&QObject::staticMetaObject;km.cast=f.common.proxy.cast;km.unblockable=[](const void*,int,unsigned){return false;};km.is_fn=[](const void*,int k,unsigned,int fn){return (k==Qt::Key_A&&fn==26)||(k==Qt::Key_B&&fn==28)||(k==Qt::Key_C&&fn==6);};for(int invalid=0;invalid<14;invalid++){auto badK=kp;auto badM=km;switch(invalid){case 0:badM.instance=nullptr;break;case 1:badM.meta=nullptr;break;case 2:badM.cast=nullptr;break;case 3:badM.is_fn=nullptr;break;case 4:badM.unblockable=nullptr;break;case 5:badK.window=nullptr;break;case 6:badK.active=nullptr;break;case 7:badK.attached_object=nullptr;break;case 8:badK.keys_call=nullptr;break;case 9:badK.key.attached=nullptr;break;case 10:badK.key.complete=nullptr;break;case 11:badK.key.event=nullptr;break;case 12:badK.key.cast=nullptr;break;case 13:badK.key.meta_event=nullptr;break;}C(!active->configure_keys(badK,badM));}C(active->configure_keys(kp,km));C(active->tick(root,100,1280,960)==0);win.show();auto*popup=root->findChild<QQuickItem*>("X2dNativeFocusPopup");C(popup&&popup->property("keyOverride").toInt()==2);popup->forceActiveFocus();QCoreApplication::processEvents();C(active->tick(root,150,1280,960)==0);QKeyEvent next(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier);QCoreApplication::sendEvent(&win,&next);C(next.isAccepted());QKeyEvent accept(QEvent::KeyPress,Qt::Key_B,Qt::NoModifier);before=writes;QCoreApplication::sendEvent(&win,&accept);C(accept.isAccepted()&&writes==before+1);C(active->tick(root,200,1280,960)==0&&!active->attached());active->close();delete active;active=nullptr;delete root;

 root=make();model=root->findChild<QObject*>("FocusModelFixture");model->setProperty("focusModeSelectable",false);C(active->tick(root,200,1280,960)==0);before=writes;C(active->key(FocusKey::Next)&&active->key(FocusKey::Accept));C(writes==before);active->close();delete active;active=nullptr;delete root;
 root=make();orig=root->findChild<QQuickItem*>("PopoverFocusMode_root");orig->setProperty("acknowledge",false);before=writes;C(active->key(FocusKey::Next)&&active->key(FocusKey::Accept));auto*nextRoot=scene();C(active->tick(nextRoot,200,1280,960)==0&&active->attached());C(writes==before+1);orig->setProperty("focusMode",2);C(active->tick(nextRoot,300,1280,960)==0&&active->attached());C(nextRoot->findChild<QObject*>("FocusModelFixture")->property("mainState").toString()=="focus_mode");active->close();delete active;active=nullptr;delete root;delete nextRoot;

 // Instrument real Qt childItems, counting only the top-level full traversal.
 root=make();C(active->tick(root,116,1280,960)==0);countRoot=root;rootScans=0;for(int i=1;i<=600;i++)C(active->tick(root,116+i*16,1280,960)==0);C(rootScans==0);before=writes;auto scans=rootScans;C(active->key(FocusKey::Next)&&active->key(FocusKey::Accept));C(rootScans==scans+1&&writes==before+1);active->close();delete active;active=nullptr;delete root;countRoot=nullptr;
 // Replacing the original viewModel must invalidate the cache on the next tick.
 root=make();auto*control=root->findChild<QQuickItem*>("ControlScreen_focusScope");QQmlComponent mc(&engine);mc.setData("import QtQuick\nQtObject { property string mainState: \"focus_mode\"; property bool focusModeSelectable: false }",QUrl());auto*replacement=mc.create(engine.rootContext());C(replacement);replacement->setParent(control);C(control->setProperty("viewModel",QVariant::fromValue<QObject*>(replacement)));countRoot=root;rootScans=0;C(active->tick(root,116,1280,960)==0&&rootScans==1);before=writes;C(active->key(FocusKey::Next)&&active->key(FocusKey::Accept));C(writes==before);active->close();delete active;active=nullptr;delete root;countRoot=nullptr;
 // Loader item is re-read on every tick without periodic full-tree scanning.
 root=make();C(active->tick(root,116,1280,960)==0);auto*loader=root->findChild<QObject*>("ControlScreen_popupLoader");C(loader->setProperty("item",QVariant::fromValue<QQuickItem*>(nullptr)));countRoot=root;rootScans=0;C(active->tick(root,132,1280,960)==0&&!active->attached()&&rootScans==0);before=writes;C(!active->key(FocusKey::Accept)&&writes==before);active->close();delete active;active=nullptr;delete root;countRoot=nullptr;
 // Foreign duplicate/renamed route introduced between scans cannot use cached setter.
 for(int variant=0;variant<2;variant++){root=make();control=root->findChild<QQuickItem*>("ControlScreen_focusScope");QQuickItem*foreign=nullptr;if(!variant){foreign=new QQuickItem(root);foreign->setObjectName("ControlScreen_focusScope");}else control->setObjectName("ForeignChangedName");countRoot=root;rootScans=0;before=writes;C(active->key(FocusKey::Next));C(active->key(FocusKey::Accept));C(rootScans==1&&writes==before);active->close();delete active;active=nullptr;delete root;countRoot=nullptr;}
 // Renamed or visually removed routes invalidate a stable cache without waiting.
 for(int variant=0;variant<2;++variant){root=make();C(active->tick(root,116,1280,960)==0);control=root->findChild<QQuickItem*>("ControlScreen_focusScope");countRoot=root;rootScans=0;before=writes;if(variant)control->setParentItem(nullptr);else control->setObjectName("RenamedControl");C(active->tick(root,132,1280,960)==0&&!active->attached()&&!active->has_source()&&rootScans==1&&writes==before);active->close();delete active;active=nullptr;delete root;countRoot=nullptr;}
 // A page loaded after an initially empty scene is discovered via childrenChanged.
 root=new QQuickItem(win.contentItem());active=new FocusAttachment(api,stock,menu,binding,sig,f,p,{});C(active->tick(root,100,1280,960)==0&&!active->has_source());auto*later=scene();later->setParentItem(root);C(active->tick(root,116,1280,960)==0&&active->attached());C(active->tick(root,132,1280,960)==0);countRoot=root;rootScans=0;for(unsigned j=0;j<80;++j){auto*extra=new QQuickItem(root);C(active->tick(root,148+j*32,1280,960)==0);delete extra;C(active->tick(root,164+j*32,1280,960)==0);}C(rootScans==160);rootScans=0;for(unsigned j=0;j<100;++j)C(active->tick(root,3000+j*16,1280,960)==0);C(rootScans==0);active->close();delete active;active=nullptr;delete later;delete root;countRoot=nullptr;
 // 完整控制页加载其它模块后超过 1024 个节点，合法唯一路由仍应可用。
 root=scene();for(unsigned i=0;i<1200;i++)new QQuickItem(root);
 active=new FocusAttachment(api,stock,menu,binding,sig,f,p,{});
 C(active->tick(root,100,1280,960)==0&&active->attached());
 C(active->tick(root,400,1280,960)==0&&active->attached());
 active->close();delete active;active=nullptr;delete root;
 // 快照失败只能恢复原厂一次，同一弹窗不可每帧重新接管造成闪烁。
 root=make();orig=root->findChild<QQuickItem*>("PopoverFocusMode_root");language=256;
 C(active->tick(root,200,1280,960)==7&&!active->attached());C(active->last_rc()==7);
 auto faults=active->failures();language=8;
 for(unsigned i=0;i<20;i++)C(active->tick(root,250+i*50,1280,960)==0&&!active->attached());
 C(active->failures()==faults&&orig->isVisible()&&orig->isEnabled());
 active->close();delete active;active=nullptr;delete root;
#include "test_focus_first_paint.inc"
#include "test_dashboard_icons.inc"
 printf("FOCUS_ATTACHMENT_PASS assertions=%u hardware_calls=0 language_proxy=fixture original_list=Qt_fixture\n",checks);
}
