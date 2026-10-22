#include "stock_metrics.hpp"
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_abi_profile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QResource>
#include <cmath>
#include <cstdio>
#include <cstdlib>
static unsigned checks;static void check(bool v,const char*s){++checks;if(!v){fprintf(stderr,"STOCK_METRICS_FAIL %s\n",s);exit(1);}}
static bool near(double a,double b){return std::abs(a-b)<0.001;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);check(argc==2&&QResource::registerResource(QString::fromLocal8Bit(argv[1])),"rcc");QQmlEngine e;QQuickWindow win;QQmlEngine::setContextForObject(win.contentItem(),e.rootContext());QQmlComponent c(&e);c.setData("import QtQuick\nimport com.hasselblad.constants\nQtObject {property var constants:Constants}",QUrl());auto*holder=c.create();check(holder,"holder");auto*constants=holder->property("constants").value<QObject*>();check(constants,"constants");auto abi=qt_abi_profile();x2dui::StockMetrics metrics(qt_stock_profile(),qt_menu_profile(),&abi);x2dui::StockMetricsSnapshot out;
for(int i=0;i<128;i++){double s=0.5+(i%8)*0.25,sx=0.5+(i%7)*0.3,sy=0.5+(i%6)*0.4;constants->setProperty("scaleFactor",s);constants->setProperty("scaleFactorX",sx);constants->setProperty("scaleFactorY",sy);app.processEvents();check(metrics.read(win.contentItem(),i%2,out),"read");check(near(out.scale,s)&&near(out.scaleX,sx)&&near(out.scaleY,sy),"scales");check(near(out.settings.headerHeight,(i%2?89.6:115)*sy),"header");check(near(out.settings.rowHeight,102.4*sy)&&near(out.settings.leftMargin,51.2*sx),"row");check(near(out.settings.labelFont,51.2*s)&&near(out.settings.valueFont,51*s),"font");check(near(out.settings.selectorLeft,320*sx)&&near(out.settings.selectorTop,131.2*sy),"selector");auto*item=static_cast<QQuickItem*>(metrics.item());check(item&&!item->isVisible()&&!item->isEnabled()&&!item->hasActiveFocus(),"hidden");check(win.contentItem()->findChildren<QQuickItem*>("X2dNativeMetricBindings").size()==1,"one container");}
constants->setProperty("scaleFactor",0);app.processEvents();check(!metrics.read(win.contentItem(),false,out),"zero rejected");constants->setProperty("scaleFactor",1);constants->setProperty("scaleFactorX",5);app.processEvents();check(!metrics.read(win.contentItem(),false,out),"large rejected");constants->setProperty("scaleFactorX",1);auto*root=new QQuickItem(win.contentItem());QQmlEngine::setContextForObject(root,e.rootContext());check(metrics.read(root,false,out),"new root");delete root;check(metrics.item()==nullptr,"root destruction");check(metrics.read(win.contentItem(),true,out),"recreate");metrics.close();check(metrics.item()==nullptr,"close");check(win.contentItem()->findChildren<QQuickItem*>("X2dNativeMetricBindings").empty(),"only own removed");delete holder;printf("NATIVE_STOCK_METRICS_PASS assertions=%u original_qrc=4.2.0 Constants=explicit_dynamic_fixture camera_requests=0\n",checks);}
