#include "binding_lease.hpp"
#include "qt_binding_profile.hpp"
#include "qt_stock_profile.hpp"
#include "qt_signal_profile.hpp"
#include "qt_abi_profile.hpp"
#include <QQuickItem>
#include <cstring>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QVariant>
#include <cstdio>
#include <cstdlib>
static unsigned checks;static void check(bool b,const char*n){++checks;if(!b){fprintf(stderr,"BINDING_LEASE_FAIL %s\n",n);exit(1);}}
int main(int argc,char**argv){QGuiApplication app(argc,argv);QQmlEngine engine;QQmlComponent c(&engine);c.setData("import QtQuick\nItem { property int base:5; property int target:base*2; property int unbound:42; property string label:\"original\"; function foreign(){target=Qt.binding(function(){return base+100})} }",QUrl());QObject*o=c.create();check(o,"QML-fixture");auto s=qt_stock_profile();auto p=qt_binding_profile();auto sig=qt_signal_profile();x2dui::BindingLease lease;
 check(lease.acquire(s,p,sig,o,u"target")&&lease.number(77),"take-original-binding");o->setProperty("base",9);check(o->property("target")==77,"original-binding-suspended");check(lease.close()&&o->property("target")==18,"restore-binding-current-evaluation");o->setProperty("base",10);check(o->property("target")==20,"binding-remains-live");
 check(lease.acquire(s,p,sig,o,u"unbound")&&lease.number(88),"unbound-lease");check(lease.close()&&o->property("unbound")==42,"unbound-original-restored");
 check(lease.acquire(s,p,sig,o,u"target")&&lease.number(77),"foreign-value-setup");o->setProperty("target",88);check(!lease.number(99)&&!lease.close()&&o->property("target")==88,"foreign-value-not-overwritten");
 check(QMetaObject::invokeMethod(o,"foreign"),"foreign-binding-created");check(lease.acquire(s,p,sig,o,u"target")&&lease.number(77),"foreign-binding-setup");check(QMetaObject::invokeMethod(o,"foreign"),"new-foreign-binding");check(!lease.number(99)&&!lease.close()&&o->property("target")==110,"foreign-binding-preserved");o->setProperty("base",11);check(o->property("target")==111,"foreign-binding-still-live");
 auto abi=qt_abi_profile();auto view=x2dui::abi_view_api(&abi);auto*textItem=static_cast<QObject*>(view.create(view.context,x2dui::Kind::Text,o));check(textItem,"native-text-item");
 for(unsigned i=0;i<128;++i){char16_t*buffer=new char16_t[64];const char16_t*initial=i%2?u"中文堆栈界面":u"ORIGINAL";size_t n=0;while(initial[n]){buffer[n]=initial[n];++n;}buffer[n]=0;QString expectedText=QString::fromUtf16(initial);
  view.text(view.context,textItem,buffer,32,1);buffer[0]=u'X';delete[]buffer;check(textItem->property("text").toString()==expectedText,"api-text-owns-freed-UTF16");
  char16_t stockBuffer[]=u"STOCK中文";check(x2dui::stock_set_text(s,textItem,"text",stockBuffer),"stock-text-set");stockBuffer[0]=u'X';check(textItem->property("text").toString()==QString::fromUtf16(u"STOCK中文"),"stock-text-owns-mutable-buffer");
  check(lease.acquire(s,p,sig,o,u"label"),"text-lease-acquire");char16_t leaseBuffer[]=u"LEASE中文";check(lease.text(leaseBuffer),"text-lease-write");leaseBuffer[0]=u'X';app.processEvents();check(o->property("label").toString()==QString::fromUtf16(u"LEASE中文"),"lease-text-owns-mutable-buffer");check(lease.close()&&o->property("label").toString()=="original","text-lease-restores-original");
 }
 check(!lease.acquire(s,p,sig,o,u"missing"),"missing-property-rejected");check(lease.acquire(s,p,sig,o,u"target"),"destroyed-target-setup");delete o;check(!lease.alive()&&!lease.number(1)&&!lease.close(),"destroyed-target-safe-release");
 printf("NATIVE_BINDING_LEASE_PASS assertions=%u actual_Qt_binding=1 camera_requests=0\n",checks);
}
