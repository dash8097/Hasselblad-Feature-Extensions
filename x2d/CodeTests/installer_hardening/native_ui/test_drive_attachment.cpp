#include "drive_attachment.hpp"
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_abi_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_key_profile.hpp"
#include "qt_binding_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <QKeyEvent>
#include <QtQuick/private/qquickitem_p.h>
#include <QTest>
#include <QDir>
#include <QFontDatabase>
#include <QRawFont>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static bool visibleText(QObject*root,const char16_t*value){auto text=QString::fromUtf16(value);for(auto*o:root->findChildren<QQuickItem*>())if(o->isVisible()&&o->property("text").toString()==text)return true;return false;}
static unsigned checks,propertyWrites;static bool(*originalSet)(void*,const char*,const void*);
static bool countedSet(void*o,const char*n,const void*v){++propertyWrites;return originalSet(o,n,v);}
static int allocationBudget=-1,allocationHits=0;
extern "C" void*__real_malloc(size_t);
extern "C" void*__wrap_malloc(size_t n){if(allocationBudget>=0&&allocationBudget--==0){++allocationHits;return nullptr;}return __real_malloc(n);}
#define CK(v) do{++checks;if(!(v)){fprintf(stderr,"DRIVE_ATTACHMENT_FAIL file=%s line=%d checks=%u\n",__FILE__,__LINE__,checks);exit(1);}}while(0)
static void replies(PixelController&c){PixelRequest r;unsigned n=0;while(c.take_request(r)){CK(++n<32);ProtocolText b;if(r.endpoint==PixelEndpoint::Status)b="{\"protocol\":3,\"session\":\"1234abcd\",\"ack\":0,\"heartbeat\":1,\"busy\":false,\"ready\":true,\"stage\":\"idle\"}";else if(r.endpoint==PixelEndpoint::Shutter)b="{\"protocol\":1,\"ready\":true,\"arm\":\"\",\"pending\":false}";else if(r.endpoint==PixelEndpoint::QuarterStatus)b="QUARTER_NATIVE_READY";else if(!r.write)b="DISARMED\n";c.response(r.id,200,b);}}
static void ready(PixelController&c){c.set_exposure_mode(0);c.set_image_format(0);c.tick(0);replies(c);c.tick(1);replies(c);CK(c.state().can_start());}
static QQuickItem*fixture(QQmlEngine&e,QQuickItem*parent){QQmlComponent component(&e);component.setData(R"(
import QtQuick
Item {id:scene; objectName:"fixtureScene";width:1024;height:768
 Item {id:control;objectName:"ControlScreen_focusScope";width:1024;height:768
  property QtObject viewModel: QtObject {property string mainState:"drive_mode";property int driveMode:0;property int expMode:0;property bool lensHasMicroFocusAdjust:true;property bool screenEVF:false;property string aperture:"f/4";property string shutterSpeed:"1/100";property string isoAsString:"100"}
  Item {objectName:"ControlScreen_popupLoader";property Item item:popup;signal loaded()
   Item {id:popup;objectName:"PopoverDriveMode_root";width:1024;height:768
    Item {id:grid;objectName:"DriveModeGrid_root";property int currentIndex:0;signal openDialog(string settingsList);onOpenDialog:(settingsList)=>{swipe.show=true;swipe.settingsList=settingsList}}
    Item {id:swipe;objectName:"PopoverDriveMode_swipe_area";property bool show:false;property string settingsList:""}
    Item {objectName:"fixtureSummary";property string settingsList:"";property string description:"fixture";property string headerText:"fixture"
      Item {property string text:"synthetic label";property string valueText:"synthetic value"}
    }
   }
  }
 }
})",QUrl());auto*r=qobject_cast<QQuickItem*>(component.create());if(!r)fprintf(stderr,"%s\n",qPrintable(component.errorString()));CK(r);r->setParent(parent);r->setParentItem(parent);return r;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);CK(argc==3&&QResource::registerResource(QString::fromLocal8Bit(argv[1])));QQmlEngine engine;QQuickWindow window;window.resize(1024,768);window.show();QQmlEngine::setContextForObject(window.contentItem(),engine.rootContext());auto stock=qt_stock_profile();originalSet=stock.set;stock.set=countedSet;auto menu=qt_menu_profile();auto abi=qt_abi_profile();auto signals=qt_signal_profile();auto binding=qt_binding_profile();auto keys=qt_key_profile();QObject map;void*mapSlot=&map;KeyMapProfile keymap{&mapSlot,&QObject::staticMetaObject,[](const void*m,const void*o)->const void*{return static_cast<const QMetaObject*>(m)->cast(static_cast<const QObject*>(o));},[](const void*,int key,unsigned,int fn){if(fn==1)return key==Qt::Key_F1;if(fn==8)return key==Qt::Key_D;return (fn==5||fn==6||fn==31)?key==Qt::Key_Escape:key==fn;},[](const void*,int key,unsigned){return key==Qt::Key_F12;}};
 FocusProfile focus{[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->window();},[](const void*w)->void*{return static_cast<const QQuickWindow*>(w)->activeFocusItem();},[](void*o,int r){static_cast<QQuickItem*>(o)->forceActiveFocus(static_cast<Qt::FocusReason>(r));}};
 DriveAttachmentProfile profile;profile.method_index=[](const void*m,const char*n){return static_cast<const QMetaObject*>(m)->indexOfMethod(n);};profile.metacall=[](void*o,int c,int index,void**args){return QMetaObject::metacall(static_cast<QObject*>(o),static_cast<QMetaObject::Call>(c),index,args);};for(int i=0;i<6;i++)profile.drive[i]=i;profile.fullAuto=4;profile.halfOverride=2;profile.keyUp[0]=Qt::Key_Up;profile.keyUp[1]=-1;profile.keyDown[0]=Qt::Key_Down;profile.keyDown[1]=-1;profile.keyLeft[0]=Qt::Key_Left;profile.keyLeft[1]=-1;profile.keyRight[0]=Qt::Key_Right;profile.keyRight[1]=-1;profile.keyAccept[0]=Qt::Key_Return;profile.keyAccept[1]=-1;profile.keyParameters=Qt::Key_F2;
 DriveStyle ds{1,1,1};SettingsStyle ss;ss.headerHeight=115;ss.rowHeight=102.4;ss.leftMargin=ss.rightMargin=51.2;ss.labelFont=51.2;ss.valueFont=51;ss.headerFont=38;ss.selectorLeft=320;ss.selectorRight=40;ss.selectorTop=ss.selectorBottom=131.2;ss.visibleChoices=4.3;
 CK(QFontDatabase::addApplicationFont(QString::fromLocal8Bit(argv[2])+"/private-font.ttc")>=0);
 QFont testFont("Microsoft YaHei");app.setFont(testFont);CK(QRawFont::fromFont(testFont).supportsCharacter(0x5806));
 auto key=[&](int k){QKeyEvent event(QEvent::KeyPress,k,Qt::NoModifier);QGuiApplication::sendEvent(&window,&event);app.processEvents();return event.isAccepted();};
 for(int scenario=0;scenario<15;scenario++){
  PixelController c;ready(c);PixelHost h(c);DriveSettingsAttachment attachment(stock,menu,&abi,signals,binding,keys,keymap,focus,profile,c,h);auto*scene=fixture(engine,window.contentItem());auto*control=scene->findChild<QQuickItem*>("ControlScreen_focusScope");auto*model=static_cast<QObject*>(stock_get_object(stock,control,"viewModel"));auto*original=scene->findChild<QQuickItem*>("PopoverDriveMode_root");original->forceActiveFocus();app.processEvents();CK(attachment.tick(scene,10,{},ds,ss,1024,768));CK(attachment.attached()&&!original->isVisible()&&!original->isEnabled());CK(window.activeFocusItem()==attachment.item());CK(attachment.tick(scene,11,{},ds,ss,1024,768));CK(scene->findChildren<QQuickItem*>("X2dNativeDriveMode").size()==1);
  if(scenario==0){unsigned before=propertyWrites;for(int idle=0;idle<100;idle++)CK(attachment.tick(scene,11,{},ds,ss,1024,768));CK(propertyWrites==before);}
  if(scenario==0){CK(key(Qt::Key_Down));CK(key(Qt::Key_Down));CK(key(Qt::Key_Down));CK(key(Qt::Key_F2));CK(attachment.tick(scene,12,{},ds,ss,1024,768));CK(scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));CK(key(Qt::Key_Down));CK(key(Qt::Key_Return));CK(attachment.tick(scene,13,{},ds,ss,1024,768));CK(key(Qt::Key_Escape));CK(key(Qt::Key_Escape));CK(attachment.tick(scene,14,{},ds,ss,1024,768));CK(!scene->findChild<QQuickItem*>("X2dNativeMultishotSettings"));}
  if(scenario==1){CK(key(Qt::Key_Right));CK(key(Qt::Key_F1));CK(static_cast<QQuickItem*>(attachment.item())->isVisible());replies(c);DriveSessionIntent intent;CK(attachment.take_intent(intent,12)&&intent.kind==DriveSessionIntentKind::AwaitNormalMode);CK(!attachment.take_intent(intent,12));CK(attachment.complete(intent.id,DriveSessionResult::Confirmed,12));CK(attachment.tick(scene,13,{},ds,ss,1024,768));CK(static_cast<QQuickItem*>(attachment.item())->isVisible());CK(attachment.take_intent(intent,13)&&intent.kind==DriveSessionIntentKind::SetDrive&&intent.choice==DriveChoice::Continuous);CK(attachment.complete(intent.id,DriveSessionResult::Unknown,13));for(int i=0;i<10;i++){attachment.tick(scene,14+i,{},ds,ss,1024,768);CK(!attachment.take_intent(intent,14+i));}CK(attachment.error()[0]);CK(visibleText(scene,attachment.error()));CK(key(Qt::Key_Escape));CK(!attachment.take_intent(intent,25));CK(model->property("mainState").toString().isEmpty());CK(!attachment.tick(scene,26,{},ds,ss,1024,768));}
  if(scenario==2){CK(key(Qt::Key_Down));CK(key(Qt::Key_F2));DriveSessionIntent intent;CK(!attachment.take_intent(intent,12));CK(scene->findChild<QObject*>("PopoverDriveMode_swipe_area")->property("show").toBool());CK(original->isVisible()&&original->isEnabled());CK(scene->findChild<QObject*>("PopoverDriveMode_swipe_area")->setProperty("show",false));CK(attachment.tick(scene,13,{},ds,ss,1024,768));CK(!original->isVisible()&&!original->isEnabled());}
  if(scenario>=3&&scenario<=5){const char*names[]={"DriveModeGrid_root","PopoverDriveMode_swipe_area","fixtureSummary"};delete scene->findChild<QObject*>(names[scenario-3]);CK(!attachment.tick(scene,12,{},ds,ss,1024,768));CK(!attachment.attached());}
  if(scenario==6){DriveSessionIntent intent;CK(attachment.dispatch_host_action({PixelHostActionKind::SetStockSingle,{}},12));CK(attachment.take_intent(intent,13)&&intent.kind==DriveSessionIntentKind::SetDrive&&intent.choice==DriveChoice::Single);CK(!attachment.take_intent(intent,13));CK(attachment.complete(intent.id,DriveSessionResult::Unknown,14));CK(attachment.error()[0]&&!attachment.take_intent(intent,15));CK(attachment.tick(scene,15,{},ds,ss,1024,768));CK(visibleText(scene,attachment.error()));CK(key(Qt::Key_Down));CK(key(Qt::Key_Down));CK(key(Qt::Key_Down));CK(visibleText(scene,attachment.error()));auto selected=h.state().selected;CK(key(Qt::Key_Escape));CK(!attachment.take_intent(intent,16));CK(model->property("mainState").toString().isEmpty()&&h.state().selected==selected);CK(!attachment.tick(scene,17,{},ds,ss,1024,768));}
  if(scenario==7){CK(attachment.dispatch_host_action({PixelHostActionKind::ClosePopup,{}},12));CK(model->property("mainState").toString().isEmpty());CK(!attachment.tick(scene,13,{},ds,ss,1024,768));CK(original->isVisible()&&original->isEnabled());}
  if(scenario==8){delete scene;scene=nullptr;CK(!attachment.attached());CK(!attachment.dispatch_host_action({PixelHostActionKind::ClosePopup,{}},12));CK(!attachment.tick(nullptr,13,{},ds,ss,1024,768));}
  if(scenario==9){delete model;CK(!attachment.tick(scene,12,{},ds,ss,1024,768));CK(!attachment.attached());}
  if(scenario==10){CK(original->setProperty("visible",true));CK(!attachment.tick(scene,12,{},ds,ss,1024,768));CK(original->isVisible());CK(!attachment.tick(scene,13,{},ds,ss,1024,768));CK(original->isVisible());}
  if(scenario==11){DriveSessionIntent intent;CK(attachment.dispatch_host_action({PixelHostActionKind::SetStockSingle,{}},12));CK(!attachment.take_intent(intent,5012));CK(attachment.error()[0]);CK(!attachment.take_intent(intent,5013));}
  if(scenario==12){DriveSessionIntent intent;CK(attachment.dispatch_host_action({PixelHostActionKind::SetStockSingle,{}},12));delete scene->findChild<QObject*>("DriveModeGrid_root");CK(!attachment.take_intent(intent,13));}
  if(scenario==13){DriveSessionIntent intent;CK(attachment.dispatch_host_action({PixelHostActionKind::SetStockSingle,{}},12));CK(attachment.take_intent(intent,13));delete scene->findChild<QObject*>("DriveModeGrid_root");CK(!attachment.complete(intent.id,DriveSessionResult::Confirmed,14));}
  if(scenario==14){DriveSessionIntent intent;CK(attachment.dispatch_host_action({PixelHostActionKind::SetStockSingle,{}},12));CK(attachment.take_intent(intent,13));CK(attachment.complete(intent.id,DriveSessionResult::Unknown,14));CK(attachment.tick(scene,15,{},ds,ss,1024,768));CK(visibleText(scene,attachment.error()));for(int i=0;i<8;i++){CK(attachment.tick(scene,16+i,{},ds,ss,1024,768));CK(!attachment.take_intent(intent,16+i));}CK(key(Qt::Key_Right));CK(key(Qt::Key_Return));CK(!attachment.error()[0]);replies(c);CK(attachment.take_intent(intent,24)&&intent.kind==DriveSessionIntentKind::AwaitNormalMode);CK(!attachment.take_intent(intent,24));CK(attachment.complete(intent.id,DriveSessionResult::Confirmed,24));CK(attachment.tick(scene,25,{},ds,ss,1024,768));CK(attachment.take_intent(intent,25)&&intent.kind==DriveSessionIntentKind::SetDrive&&intent.choice==DriveChoice::Continuous);CK(!attachment.take_intent(intent,25));}
  attachment.close();if(scene){CK(original->isVisible()&&original->isEnabled());CK(!scene->findChild<QQuickItem*>("X2dNativeDriveMode"));delete scene;}attachment.close();
 }
#include "test_multishot_reentry.inc"
#include "test_multishot_touch.inc"
#include "test_multishot_settings_keys.inc"
 // Unrelated application pages must not become a dependency of drive UI.
 // More than the old global observer's 4096-node limit, plus page churn.
 {
  PixelController c;ready(c);PixelHost h(c);DriveSettingsAttachment attachment(stock,menu,&abi,signals,binding,keys,keymap,focus,profile,c,h);
  auto*scene=fixture(engine,window.contentItem());auto*control=scene->findChild<QQuickItem*>("ControlScreen_focusScope");auto*model=static_cast<QObject*>(stock_get_object(stock,control,"viewModel"));
  model->setProperty("mainState","");CK(!attachment.tick(scene,10,{},ds,ss,1024,768));
  auto*unrelated=new QQuickItem(scene);unrelated->setParentItem(scene);for(int n=0;n<4200;++n)new QQuickItem(unrelated);
  for(int n=0;n<30;++n){auto*item=new QQuickItem(unrelated);delete item;CK(!attachment.tick(scene,11+n,{},ds,ss,1024,768));}
  model->setProperty("mainState","drive_mode");CK(attachment.tick(scene,50,{},ds,ss,1024,768));CK(attachment.attached());
  model->setProperty("mainState","");CK(!attachment.tick(scene,51,{},ds,ss,1024,768));CK(!attachment.attached());
  // Destroy the cached source, then replace it without changing the scene.
  delete unrelated;delete control;CK(!attachment.dispatch_host_action({PixelHostActionKind::ClosePopup,{}},52));
  auto*replacement=fixture(engine,scene);CK(attachment.tick(scene,53,{},ds,ss,1024,768));CK(attachment.attached());
  auto*replacementControl=replacement->findChild<QQuickItem*>("ControlScreen_focusScope");auto*replacementModel=static_cast<QObject*>(stock_get_object(stock,replacementControl,"viewModel"));
  CK(attachment.dispatch_host_action({PixelHostActionKind::ClosePopup,{}},54));CK(replacementModel->property("mainState").toString().isEmpty());
  CK(!attachment.tick(scene,55,{},ds,ss,1024,768));attachment.close();delete scene;
 }
#include "test_drive_first_paint.inc"
 unsigned positions=0;for(int budget=0;budget<96;++budget){PixelController c;ready(c);PixelHost h(c);DriveSettingsAttachment attachment(stock,menu,&abi,signals,binding,keys,keymap,focus,profile,c,h);auto*scene=fixture(engine,window.contentItem());auto*original=scene->findChild<QQuickItem*>("PopoverDriveMode_root");allocationBudget=budget;allocationHits=0;bool ok=attachment.tick(scene,10,{},ds,ss,1024,768);allocationBudget=-1;if(allocationHits){++positions;CK(!ok);}attachment.close();CK(original->isVisible()&&original->isEnabled());CK(!scene->findChild<QQuickItem*>("X2dNativeDriveMode"));delete scene;}CK(positions>0);
 printf("DRIVE_ATTACHMENT_PASS assertions=%u actual_malloc_failure_positions=%u Qt=6.4.1 original_controls=fixtures original_model=synthetic camera_operations=0\n",checks,positions);
}
