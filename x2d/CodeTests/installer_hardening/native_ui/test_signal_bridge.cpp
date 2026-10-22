#include "signal_bridge.hpp"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QMetaMethod>
#include <QLibrary>
#include <cstdio>
#include <cstdlib>
static int checks,hits,releases;static void check(bool b,const char*n){++checks;if(!b){fprintf(stderr,"SIGNAL_FAIL %s\n",n);exit(1);}}
template<class F>F fn(const char*n){auto p=QLibrary::resolve("Qt6Core",n);check(p,n);return reinterpret_cast<F>(p);}
static void callback(void*){++hits;}static void release(void*){++releases;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);static_assert(sizeof(QMetaMethod)==16,"actual Qt method ABI");x2dui::SignalProfile p{};
 p.index=fn<decltype(p.index)>("_ZNK11QMetaObject13indexOfSignalEPKc");p.method=fn<decltype(p.method)>("_ZNK11QMetaObject6methodEi");p.signal_index=fn<decltype(p.signal_index)>("_ZN18QMetaObjectPrivate11signalIndexERK11QMetaMethod");p.connect=fn<decltype(p.connect)>("_ZN14QObjectPrivate7connectEPK7QObjectiS2_PN9QtPrivate15QSlotObjectBaseEN2Qt14ConnectionTypeE");p.disconnect=fn<decltype(p.disconnect)>("_ZN7QObject10disconnectERKN11QMetaObject10ConnectionE");p.connection_dtor=fn<decltype(p.connection_dtor)>("_ZN11QMetaObject10ConnectionD1Ev");
 QQuickItem sender;auto*receiver=new QQuickItem;
 check(!x2dui::signal_watch(p,&sender,receiver,"notASignal()",callback,nullptr,release)&&!releases,"missing-signal-keeps-context");
 auto*w=x2dui::signal_watch(p,&sender,receiver,"objectNameChanged(QString)",callback,nullptr,release);check(w,"base-notify");sender.setObjectName("a");check(hits==1&&!releases,"notify-delivered");x2dui::signal_unwatch(w);check(releases==1,"explicit-disconnect-release");sender.setObjectName("b");check(hits==1,"no-call-after-disconnect");
 w=x2dui::signal_watch(p,&sender,receiver,"childrenChanged()",callback,nullptr,release);check(w,"item-signal");auto*child=new QQuickItem(&sender);check(hits==2,"child-notify");delete receiver;check(releases==2,"receiver-destroy-releases");delete child;check(hits==2,"no-call-after-receiver-destroy");x2dui::signal_unwatch(w);check(releases==2,"stale-connection-safe-unwatch");
 QQmlEngine engine;QQmlComponent component(&engine);component.setData("import QtQuick\nItem {property string menuName: \"one\"}",QUrl());auto*object=component.create();check(object,"private-QML-fixture");QQuickItem owner;
 w=x2dui::signal_watch(p,object,&owner,"menuNameChanged()",callback,nullptr,release);check(w,"dynamic-QML-signal");object->setProperty("menuName","two");check(hits==3,"dynamic-notify");delete object;check(releases==3,"sender-destroy-releases");x2dui::signal_unwatch(w);check(releases==3,"sender-dead-safe-unwatch");
 printf("NATIVE_SIGNAL_BRIDGE_PASS assertions=%d real_Qt=1 QML_fixture_only=1 camera_requests=0\n",checks);
}
