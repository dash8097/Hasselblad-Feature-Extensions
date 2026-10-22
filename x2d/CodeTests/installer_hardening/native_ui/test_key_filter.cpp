#include "qt_key_profile.hpp"
#include "qt_signal_profile.hpp"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QKeyEvent>
#include <QPointer>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <cstdio>
#include <cstdlib>
static int checks,hits;static bool consume=true;static x2dui::KeyInput last;
static void check(bool b,const char*n){++checks;if(!b){fprintf(stderr,"KEY_FILTER_FAIL %s\n",n);exit(1);}}
static bool input(void*,const x2dui::KeyInput&k){++hits;last=k;return consume;}
static bool selfClose(void*p,const x2dui::KeyInput&){auto**f=static_cast<x2dui::NativeKeyFilter**>(p);auto*old=*f;*f=nullptr;old->close();return true;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);QQuickWindow w;w.resize(800,600);auto*item=new QQuickItem(w.contentItem());item->setWidth(800);item->setHeight(600);auto*filter=x2dui::NativeKeyFilter::create(qt_key_profile(),qt_signal_profile(),item,input,nullptr);check(filter,"native-original-KeysAttached-created");w.show();item->forceActiveFocus();app.processEvents();check(item->hasActiveFocus(),"actual-active-focus");
 QKeyEvent a(QEvent::KeyPress,Qt::Key_A,Qt::ShiftModifier,"A",true);QGuiApplication::sendEvent(&w,&a);check(hits==1&&last.key==Qt::Key_A&&last.modifiers==Qt::ShiftModifier&&last.repeat,"window-event-delivered-with-arguments");check(a.isAccepted(),"consumed-accepted");
 consume=false;QKeyEvent b(QEvent::KeyPress,Qt::Key_B,Qt::NoModifier);QGuiApplication::sendEvent(&w,&b);check(hits==2&&last.key==Qt::Key_B&&!last.repeat,"next-key-independent");check(!b.isAccepted(),"passthrough-unaccepted");
 QPointer<QObject> attached=item->findChild<QQuickKeysAttached*>();check(attached,"attached-lifetime-observed");
 filter->close();check(!attached.isNull(),"item-key-chain-must-not-reference-deleted-attached-object");
 QKeyEvent c(QEvent::KeyPress,Qt::Key_C,Qt::NoModifier);QGuiApplication::sendEvent(&w,&c);check(hits==2,"disconnect-removes-key-filter");
 for(int cycle=0;cycle<100;cycle++){
  filter=x2dui::NativeKeyFilter::create(qt_key_profile(),qt_signal_profile(),item,input,nullptr);check(filter,"recreate");
  check(item->findChildren<QQuickKeysAttached*>().size()==1,"reentry-reuses-one-owned-key-object");
  consume=true;int before=hits;QKeyEvent press(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QGuiApplication::sendEvent(&w,&press);check(hits==before+1,"one-callback-per-press");
  filter->close();QKeyEvent release(QEvent::KeyRelease,Qt::Key_Return,Qt::NoModifier);QGuiApplication::sendEvent(&w,&release);check(hits==before+1,"close-between-press-release");
  QKeyEvent shortcut(QEvent::ShortcutOverride,Qt::Key_C,Qt::NoModifier);QGuiApplication::sendEvent(item,&shortcut);
 }
 filter=x2dui::NativeKeyFilter::create(qt_key_profile(),qt_signal_profile(),item,input,nullptr);check(filter,"last-recreate");delete item;check(attached.isNull(),"parent-owns-key-object");filter->close();check(true,"parent-destruction-safe-close");
 item=new QQuickItem(w.contentItem());item->forceActiveFocus();app.processEvents();auto profile=qt_key_profile();auto*original=static_cast<QQuickKeysAttached*>(profile.attached(item));profile.complete(original);int factoryHits=0;
 QObject::connect(original,&QQuickKeysAttached::pressed,[&](QQuickKeyEvent*){++factoryHits;});
 filter=x2dui::NativeKeyFilter::create(profile,qt_signal_profile(),item,selfClose,&filter);check(filter,"private-handler-coexists-with-factory");
 QKeyEvent close(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QGuiApplication::sendEvent(&w,&close);check(!filter&&close.isAccepted()&&!factoryHits,"close-inside-pressed-no-factory-duplicate");
 QKeyEvent released(QEvent::KeyRelease,Qt::Key_Escape,Qt::NoModifier);QGuiApplication::sendEvent(&w,&released);
 QKeyEvent passthrough(QEvent::KeyPress,Qt::Key_D,Qt::NoModifier);QGuiApplication::sendEvent(&w,&passthrough);check(factoryHits==1,"factory-handler-preserved-after-close");delete item;
 printf("NATIVE_KEY_FILTER_PASS assertions=%d actual_Qt_window_keys=1 camera_key_mapping=not_tested camera_requests=0\n",checks);
}
