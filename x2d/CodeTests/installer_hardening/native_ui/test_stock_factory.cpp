#include "qt_stock_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QResource>
#include <QVariant>
#include <QPointer>
#include <cstdio>
#include <cstdlib>
static int checks;static void check(bool v,const char*n){++checks;if(!v){fprintf(stderr,"STOCK_FAIL %s\n",n);exit(1);}}
int main(int argc,char**argv){QGuiApplication app(argc,argv);check(argc==2&&QResource::registerResource(QString::fromLocal8Bit(argv[1])),"private-fixture-resource");auto p=qt_stock_profile();QQmlEngine engine;QQuickItem root;
 check(!x2dui::stock_create(p,x2dui::StockKind::ExposeScreen,&root,&root),"missing-context");QQmlEngine::setContextForObject(&root,engine.rootContext());
 check(!x2dui::stock_create(p,static_cast<x2dui::StockKind>(99),&root,&root),"kind-whitelist");
 auto*item=static_cast<QQuickItem*>(x2dui::stock_create(p,x2dui::StockKind::ExposeScreen,&root,&root));check(item&&item->parentItem()==&root&&item->parent()==&root,"original-engine-created-item");
 check(x2dui::stock_set_text(p,item,"headerText",u"测试")&&item->property("headerText").toString()==QString::fromUtf8("测试"),"text-scalar-ABI");
 check(x2dui::stock_set_bool(p,item,"showButtons",true)&&item->property("showButtons").toBool(),"bool-scalar-ABI");
 check(x2dui::stock_set_number(p,item,"progress",0.75)&&item->property("progress").toDouble()==0.75,"double-scalar-ABI");
 QObject model;check(x2dui::stock_set_object(p,item,"viewModel",&model)&&x2dui::stock_get_object(p,item,"viewModel")==&model,"QObject-injection-roundtrip");
 auto*label=static_cast<QObject*>(x2dui::stock_get_object(p,item,"text1"));check(label&&x2dui::stock_set_text(p,label,"text",u"count")&&label->property("text")=="count","object-alias-child-update");
 check(!x2dui::stock_set_bool(p,item,"notAProperty",true)&&!item->dynamicPropertyNames().contains("notAProperty"),"unknown-property-no-mutation");
 check(!x2dui::stock_get_object(p,item,"progress"),"non-object-refused");
 check(!x2dui::stock_create(p,x2dui::StockKind::ExposeProgress,&root,&root),"missing-stock-resource-rejected");
 check(!x2dui::stock_create(p,x2dui::StockKind::ExposeIconText,&root,&root),"non-visual-resource-rejected");
 QPointer<QObject>guard(item);p.object_destroy(item);check(!guard&&root.childItems().empty(),"virtual-destruction");
 printf("NATIVE_STOCK_FACTORY_PASS assertions=%d real_Qt=1 original_qrc=fixture camera_requests=0\n",checks);
}
