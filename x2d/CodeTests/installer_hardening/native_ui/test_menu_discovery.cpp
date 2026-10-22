#include "qt_menu_profile.hpp"
#include "effect_menu_attachment.hpp"
#include "tree_observer.hpp"
#include "qt_signal_profile.hpp"
#include "qt_abi_profile.hpp"
#include "qt_stock_profile.hpp"
#include "qt_binding_profile.hpp"
#include "qt_key_profile.hpp"
#include <QtQuick/private/qquickitem_p.h>
#include <QtQml/qqml.h>
#include <QtTest/QTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QtQuick/private/qquicklistview_p.h>
#include <QQmlParserStatus>
#include <QVariant>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <cstdio>
#include <cstdlib>
static int checks;static void check(bool v,const char*n){++checks;if(!v){std::fprintf(stderr,"MENU_FAIL %s\n",n);std::exit(1);}}
static unsigned flashCommands;
static unsigned effectWrites;static int effectSelected=-1;
extern "C" int x2d_flash_client_set(X2dFlashClient*,unsigned,unsigned,uint64_t){++flashCommands;return 1;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);auto profile=qt_menu_profile();QQuickWindow window;auto*root=window.contentItem();x2dui::MenuTarget target;QQmlEngine engine;QQmlEngine::setContextForObject(root,engine.rootContext());QQmlComponent modelComponent(&engine);modelComponent.setData("import QtQml\nQtObject { property bool screenEVF:false }",QUrl("qrc:/evf-fixture.qml"));auto*viewModel=modelComponent.create();check(viewModel,"evf-model");
 check(x2dui::find_exposure_menu(profile,root,target)==1,"no-page");
 QQmlComponent pageComponent(&engine);pageComponent.setData("import QtQuick\nItem { property string menuName: \"otherMenu\"; property var heightForItem; property QtObject viewModel }",QUrl("qrc:/settings-fixture.qml"));auto*page=qobject_cast<QQuickItem*>(pageComponent.create());check(page,"typed-settings-page");page->setParentItem(root);page->setParent(root);page->setObjectName("SettingsGeneric_root_1");
 page->setProperty("viewModel",QVariant::fromValue(viewModel));
 auto*list=new QQuickListView(page);list->setParent(page);list->setObjectName("SettingsGeneric_list");static_cast<QQmlParserStatus*>(list)->componentComplete();
 check(x2dui::find_exposure_menu(profile,root,target)==1,"wrong-menu");page->setProperty("menuName",QString("exposureMenu"));page->setProperty("heightForItem",96.0);
 check(x2dui::find_exposure_menu(profile,root,target)==0&&target.page==page&&target.list==list&&target.content==list->contentItem()&&target.rowHeight==96,"find-typed-page");
 for(int i=0;i<1000;i++)check(x2dui::find_exposure_menu(profile,root,target)==0,"repeated-value-refcounts");
 list->setBottomMargin(12);check(x2dui::find_exposure_menu(profile,root,target)==5,"foreign-margin");list->setBottomMargin(0);
 page->setProperty("heightForItem",-10.0);check(x2dui::find_exposure_menu(profile,root,target)==6,"invalid-row-height");page->setProperty("heightForItem",QVariant());check(x2dui::find_exposure_menu(profile,root,target)==0&&target.rowHeight==88,"missing-row-default");
 auto*duplicate=new QQuickItem(root);duplicate->setParent(root);duplicate->setObjectName("SettingsGeneric_root_2");duplicate->setProperty("menuName",QString("exposureMenu"));check(x2dui::find_exposure_menu(profile,root,target)==3,"duplicate-page");delete duplicate;
 auto*foreign=new QQuickItem(page);foreign->setParent(page);foreign->setObjectName("SettingsGeneric_list");check(x2dui::find_exposure_menu(profile,root,target)==3,"duplicate-list");delete foreign;
 // Native footer geometry hypothesis: extra item goes after content, with a
 // separately owned bottom margin. This does not change the model or footer.
 list->setWidth(800);list->setHeight(600);list->setContentHeight(1000);
 auto abi=qt_abi_profile();auto*view=x2dui::EffectView::create(x2dui::abi_view_api(&abi),list->contentItem(),root,nullptr,nullptr);check(view,"native-view");view->resize(800,600);
 static_cast<QQuickItem*>(view->item())->setY(list->contentHeight());profile.set_bottom_margin(list,88);
 check(list->contentHeight()==1000&&list->bottomMargin()==88&&static_cast<QQuickItem*>(view->item())->y()==1000,"native-footer-does-not-replace-model");
 profile.set_bottom_margin(list,0);view->close();check(list->bottomMargin()==0&&list->footer()==nullptr,"footer-restore");


 x2dui::TreeObserver observer(profile,x2dui::abi_view_api(&abi),qt_signal_profile());
 check(observer.refresh(root)&&!observer.pending(),"observer-initial");unsigned watched=observer.watchedNodes();
 check(observer.refresh(root)&&observer.watchedNodes()==watched&&!observer.pending(),"observer-stable-no-rebuild");
 auto*dynamicChild=new QQuickItem(list);check(observer.pending(),"observer-tree-change");check(observer.refresh(root)&&!observer.pending(),"observer-new-tree");delete dynamicChild;check(observer.pending(),"observer-removal");check(observer.refresh(root),"observer-after-removal");
 unsigned builds=observer.subscriptionRebuilds();
 check(QMetaObject::invokeMethod(list,"movementStarted",Qt::DirectConnection),"original-flickable-start-signal");
 check(QMetaObject::invokeMethod(list,"movementStarted",Qt::DirectConnection),"duplicate-movement-start-is-idempotent");
 auto*duringScroll=new QQuickItem(list);unsigned scrollConnections=observer.subscriptionConnections();
 check(!observer.pending()&&observer.refresh(root),"defer-discovery-while-scrolling");
 check(observer.subscriptionConnections()==scrollConnections,"no-new-connections-during-scroll");
 check(QMetaObject::invokeMethod(list,"movementEnded",Qt::DirectConnection),"original-flickable-end-signal");
 check(observer.pending()&&observer.refresh(root),"reconcile-after-scroll");
 duringScroll->setHeight(19);check(observer.pending(),"scrolled-in-node-observed-after-settle");
 observer.refresh(root);delete duringScroll;observer.refresh(root);
 auto*removedScroll=new QQuickListView(page);static_cast<QQmlParserStatus*>(removedScroll)->componentComplete();check(observer.refresh(root),"observe-temporary-flickable");
 check(QMetaObject::invokeMethod(removedScroll,"movementStarted",Qt::DirectConnection),"temporary-flickable-start");
 delete removedScroll;check(observer.pending()&&observer.refresh(root),"destroy-during-scroll-unblocks-observer");
 builds=observer.subscriptionRebuilds();
 // Large menus must not compare each node with every prior subscription.
 QQuickItem*bulk[700];for(auto*&item:bulk)item=new QQuickItem(root);
 check(observer.refresh(root),"bulk-first");QMetaObject::invokeMethod(root,"childrenChanged",Qt::DirectConnection);
 check(observer.refresh(root),"bulk-repeat");check(observer.lookupComparisons()<observer.watchedNodes()*12,"bounded-hash-lookup");
 for(auto*item:bulk)delete item;check(observer.refresh(root),"bulk-remove");
 unsigned existingConnections=observer.subscriptionConnections();
 auto*incremental=new QQuickItem(list);check(observer.refresh(root),"incremental-add");
 check(observer.subscriptionConnections()-existingConnections<=14,"new-child-does-not-reconnect-existing-nodes");
 existingConnections=observer.subscriptionConnections();delete incremental;check(observer.refresh(root),"incremental-remove");
 check(observer.subscriptionConnections()==existingConnections,"remove-does-not-reconnect-existing-nodes");
 // A live item may leave the visual tree without being destroyed.
 auto*detached=new QQuickItem(list);check(observer.refresh(root),"observe-before-reparent");
 detached->setParentItem(nullptr);check(observer.refresh(root),"prune-detached-node");
 detached->setWidth(17);check(!observer.pending(),"detached-node-no-longer-invalidates");
 detached->setParentItem(list);check(observer.refresh(root),"reattach-live-node");
 detached->setWidth(18);check(observer.pending(),"reattached-node-is-observed");
 observer.refresh(root);delete detached;check(observer.refresh(root),"destroyed-node-pruned");
 builds=observer.subscriptionRebuilds();
 for(int i=0;i<100;i++){list->setWidth(801+i);check(observer.pending(),"layout-notification");check(observer.refresh(root),"layout-refresh");}
 check(observer.subscriptionRebuilds()==builds,"layout-does-not-reconnect-tree");
 auto*late=new QQuickItem(list);check(observer.refresh(root)&&observer.subscriptionRebuilds()==builds+1,"new-child-reconnects");
 late->setHeight(42);check(observer.pending(),"new-child-is-observed");observer.refresh(root);delete late;observer.refresh(root);
 list->setContentHeight(1200);check(observer.pending(),"observer-content-height");check(observer.refresh(root),"observer-height-refresh");list->setContentHeight(1000);observer.refresh(root);
 x2dui::EffectMenuAttachment attachment(profile,x2dui::abi_view_api(&abi),[](void*,int n){++effectWrites;effectSelected=n;},nullptr,qt_stock_profile(),qt_binding_profile(),qt_signal_profile());
 check(attachment.tick(root,1,true,800,600)==0&&list->bottomMargin()==88,"attachment-created");
 check(static_cast<QQuickItem*>(attachment.item())->y()==1000,"attachment-after-content");
 list->setContentHeight(1300);check(attachment.tick(root,2,true,800,600)==0&&static_cast<QQuickItem*>(attachment.item())->y()==1300,"attachment-model-growth");
 page->setProperty("heightForItem",96.0);check(attachment.tick(root,2,true,800,600)==0&&list->bottomMargin()==96&&static_cast<QQuickItem*>(attachment.item())->height()==96,"native-row-style-height");
 list->setBottomMargin(120);check(attachment.tick(root,2,true,800,600)==5&&!attachment.item()&&list->bottomMargin()==120,"foreign-margin-preserved");list->setBottomMargin(0);
 check(attachment.tick(root,2,true,800,600)==0,"reattach");page->setProperty("menuName",QString("anotherMenu"));check(attachment.tick(root,2,true,800,600)==1&&!attachment.item()&&list->bottomMargin()==0,"menu-switch-restores-owned-margin");page->setProperty("menuName",QString("exposureMenu"));
 attachment.close(root);X2dFlashClient flash{};flash.ready=1;flash.state.profile=2;flash.state.requested=1;flash.state.delay_us=160000;
 attachment.flashService(&flash,1000,{true,true,true,0,32});
 check(attachment.tick(root,2,true,800,600)==0&&list->bottomMargin()==96,"only-original-animation-footer-row");
 check(!root->findChild<QQuickItem*>("X2dNativeFlashDelay")&&!root->findChild<QQuickItem*>("X2dNativeFlashOption"),"no-electronic-flash-ui-switch-or-delay");
 check(root->findChildren<QQuickItem*>("X2dNativeEffectPopup").size()==1,"no-electronic-flash-popup");
 page->setProperty("menuName",QString("anotherMenu"));check(attachment.tick(root,2,true,800,600)==1&&list->bottomMargin()==0,"page-close-restores-margin");
 check(flashCommands==0&&flash.state.requested==1,"close-does-not-disable-automatic-flash");page->setProperty("menuName",QString("exposureMenu"));
 check(attachment.tick(root,2,true,800,600)==0,"attach-before-page-delete");delete page;page=nullptr;
 check(attachment.tick(root,2,true,800,600)==1&&!attachment.item(),"deleted-parent-clears-view");attachment.close(root);observer.reset();
 delete page;check(x2dui::find_exposure_menu(profile,root,target)==1,"destroyed-page-disappears");
 // The original SettingsGeneric uses contentHeight > height to enable touch.
 // A native row in bottomMargin must extend that condition, and then restore
 // the original binding. It must not enable touch in EVF mode.
 QQmlComponent fixture(&engine);fixture.setData(R"QML(import QtQuick
Item { id:page;objectName:"SettingsGeneric_root_scroll";width:800;height:600
 property string menuName:"exposureMenu"
 property real heightForItem:96
 property QtObject viewModel:QtObject { property bool screenEVF:false }
 property int backgroundKeys:0
 Keys.onPressed:(event)=>{backgroundKeys++;event.accepted=true}
 function removeFirst(){entries.remove(0)}
 ListModel {id:entries;ListElement{rh:110} ListElement{rh:110} ListElement{rh:110} ListElement{rh:110} ListElement{rh:110}}
 ListView {id:list;objectName:"SettingsGeneric_list";width:800;height:600;clip:true
  interactive:contentHeight>height&&!page.viewModel.screenEVF
  model:entries;delegate:Item {width:800;height:rh}
 }
})QML",QUrl("qrc:/scroll-fixture.qml"));check(fixture.isReady(),"scroll-fixture-component");
 auto*scrollPage=qobject_cast<QQuickItem*>(fixture.create());check(scrollPage,"scroll-fixture-created");scrollPage->setParentItem(root);scrollPage->setParent(root);auto*scrollList=scrollPage->findChild<QQuickListView*>("SettingsGeneric_list");check(scrollList,"scroll-fixture-list");window.resize(800,600);window.show();scrollList->forceLayout();QCoreApplication::processEvents();
 auto*evf=scrollPage->property("viewModel").value<QObject*>();check(evf,"scroll-evf-model");
 check(scrollList->contentHeight()==550&&!scrollList->isInteractive(),"original-rows-fit-but-extra-row-does-not");
 check(attachment.tick(root,2,true,800,600)==0&&scrollList->isInteractive(),"extra-row-enables-needed-touch-scroll");
 auto*extra=static_cast<QQuickItem*>(attachment.item());scrollList->setContentY(scrollList->originY()+scrollList->contentHeight()+scrollList->bottomMargin()-scrollList->height());
 auto position=extra->mapToItem(scrollList,QPointF());check(position.y()>=0&&position.y()+extra->height()<=scrollList->height(),"entire-animation-row-reachable-on-screen");
 x2dui::ScreenKeyProfile kp;kp.key=qt_key_profile();kp.attached_object=[](void*o,void*(*f)(void*),bool c)->void*{return qmlAttachedPropertiesObject(static_cast<QObject*>(o),reinterpret_cast<QQmlAttachedPropertiesFunc>(f),c);};kp.keys_call=reinterpret_cast<x2dui::AbiMetaCall>(QQuickKeysAttached::staticMetaObject.d.static_metacall);kp.window=[](const void*o)->void*{return static_cast<const QQuickItem*>(o)->window();};kp.active=[](const void*o)->void*{return static_cast<const QQuickWindow*>(o)->activeFocusItem();};
 void*keyOwner=&attachment;x2dui::KeyMapProfile km{};km.instance=&keyOwner;km.cast=[](const void*,const void*o){return o;};km.is_fn=[](const void*,int key,unsigned,int fn){return (key==Qt::Key_F4&&fn==28)||(key==Qt::Key_D&&fn==8)||(key==Qt::Key_Escape&&fn==5)||(key==Qt::Key_Up&&fn==27)||(key==Qt::Key_Down&&fn==26);};check(attachment.configure_keys(kp,km),"effect-key-profile");
 scrollPage->forceActiveFocus();QCoreApplication::processEvents();auto openEffect=[&](){QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,{100,int(position.y()+30)});attachment.refresh(2,true);QCoreApplication::processEvents();};
 auto*effectPopup=root->findChild<QQuickItem*>("X2dNativeEffectPopup");check(effectPopup,"effect-popup-for-key-test");openEffect();check(effectPopup->isVisible(),"touch-open-before-key-navigation");
 QTest::keyClick(&window,Qt::Key_Up);QTest::keyClick(&window,Qt::Key_Escape);if(effectPopup->isVisible()||effectWrites||scrollPage->property("backgroundKeys").toInt())fprintf(stderr,"effect-key-cancel visible=%d writes=%u background=%d focus=%s\n",effectPopup->isVisible(),effectWrites,scrollPage->property("backgroundKeys").toInt(),window.activeFocusItem()?window.activeFocusItem()->objectName().toUtf8().constData():"none");check(!effectPopup->isVisible()&&effectWrites==0&&scrollPage->property("backgroundKeys").toInt()==0,"cancel-does-not-leak-to-original-menu");
 openEffect();QTest::keyClick(&window,Qt::Key_Up);QTest::keyClick(&window,Qt::Key_D);check(!effectPopup->isVisible()&&effectWrites==1&&effectSelected==1&&scrollPage->property("backgroundKeys").toInt()==0,"wheel-select-applies-only-effect");
 attachment.refresh(2,true);QTest::keyClick(&window,Qt::Key_Down);check(scrollPage->property("backgroundKeys").toInt()==1,"closed-popup-restores-original-key-route");
 evf->setProperty("screenEVF",true);attachment.refresh(2,true);check(!scrollList->isInteractive(),"evf-restores-policy-without-tree-rescan");
 evf->setProperty("screenEVF",false);attachment.refresh(2,true);check(scrollList->isInteractive(),"touch-screen-can-reenable-extra-scroll-without-rescan");
 attachment.close(root);check(!scrollList->isInteractive(),"exit-restores-original-binding-value");scrollList->setHeight(500);check(scrollList->isInteractive(),"restored-binding-still-reacts-to-height");scrollList->setHeight(600);check(!scrollList->isInteractive(),"restored-binding-follows-fitting-content");
 check(attachment.tick(root,2,true,800,600)==0,"scroll-reattach");scrollList->setInteractive(false);check(attachment.tick(root,2,true,800,600)==10&&!scrollList->isInteractive(),"foreign-scroll-value-preserved");check(attachment.tick(root,2,true,800,600)==10&&!scrollList->isInteractive(),"foreign-scroll-not-reacquired-on-next-tick");scrollPage->setProperty("menuName","other");attachment.tick(root,2,true,800,600);scrollPage->setProperty("menuName","exposureMenu");
 scrollList->setHeight(250);scrollList->setContentY(220);scrollList->forceLayout();check(QMetaObject::invokeMethod(scrollPage,"removeFirst"),"remove-first-variable-list-row");scrollList->forceLayout();QCoreApplication::processEvents();check(scrollList->originY()!=0,"list-origin-moves-after-removing-leading-row");
 check(attachment.tick(root,2,true,800,600)==0,"attach-with-nonzero-list-origin");check(static_cast<QQuickItem*>(attachment.item())->y()==scrollList->originY()+scrollList->contentHeight(),"footer-uses-content-coordinate-origin");
 attachment.close(root);delete scrollPage;delete viewModel;
 std::printf("NATIVE_MENU_DISCOVERY_PASS assertions=%d QML=0 transport=none camera_requests=0\n",checks);return 0;
}
