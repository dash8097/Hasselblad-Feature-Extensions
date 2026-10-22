#include "screen_key_profile.hpp"
#include "qt_key_profile.hpp"
#include "qt_signal_profile.hpp"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QtQml/qqml.h>
#include <QKeyEvent>
#include <cstdio>
#include <cstdlib>
using namespace x2dui;
static unsigned checks,presses,releases;static bool consumed=true;static ScreenKeyReceiver*receiver=nullptr;static bool closeInCallback=false;
#define C(v) do{++checks;if(!(v)){fprintf(stderr,"SCREEN_KEYS_FAIL line=%d expr=%s\n",__LINE__,#v);exit(1);}}while(0)
static bool handler(void*,const KeyInput&k,bool release){C(k.key==Qt::Key_A);if(release)++releases;else ++presses;if(closeInCallback){auto*p=receiver;receiver=nullptr;p->close();}return consumed;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);ScreenKeyProfile p;p.key=qt_key_profile();p.attached_object=[](void*o,void*(*factory)(void*),bool create)->void*{return qmlAttachedPropertiesObject(static_cast<QObject*>(o),reinterpret_cast<QQmlAttachedPropertiesFunc>(factory),create);};p.keys_call=reinterpret_cast<AbiMetaCall>(QQuickKeysAttached::staticMetaObject.d.static_metacall);auto sig=qt_signal_profile();
 QQuickItem owner,own,a,b,c;ScreenKeyLease lease(p,sig);C(lease.attach(&owner,&own)&&lease.valid());auto*keys=static_cast<QQuickKeysAttached*>(p.attached_object(&owner,p.key.attached,false));C(keys);auto list=keys->forwardTo();C(list.count(&list)==1&&list.at(&list,0)==&own);list.append(&list,&a);list.append(&list,&b);C(lease.close());C(list.count(&list)==2&&list.at(&list,0)==&a&&list.at(&list,1)==&b);
 C(lease.attach(&owner,&own));C(list.count(&list)==3&&list.at(&list,0)==&own);list.clear(&list);list.append(&list,&b);list.append(&list,&own);list.append(&list,&c);list.append(&list,&own);C(lease.close());C(list.count(&list)==2&&list.at(&list,0)==&b&&list.at(&list,1)==&c);
 // attach限64，拆除不因foreign随后增多而遗留自身悬空地址。
 C(lease.attach(&owner,&own));for(int i=0;i<128;i++)list.append(&list,&a);C(lease.close());C(list.count(&list)==130);C(!lease.attach(&owner,&own));C(list.count(&list)==130);list.clear(&list);
 keys->setEnabled(false);C(!lease.attach(&owner,&own));C(!keys->enabled()&&list.count(&list)==0);keys->setEnabled(true);
 auto*deletedOwner=new QQuickItem;C(lease.attach(deletedOwner,&own));delete deletedOwner;C(!lease.valid()&&lease.close());auto*deletedTarget=new QQuickItem;C(lease.attach(&owner,deletedTarget));delete deletedTarget;C(!lease.valid()&&list.count(&list)==0&&lease.close());
 QQuickItem target;receiver=ScreenKeyReceiver::create(p,sig,&target,handler,nullptr);C(receiver);C(!ScreenKeyReceiver::create(p,sig,&target,handler,nullptr));auto*ownKeys=static_cast<QQuickKeysAttached*>(p.attached_object(&target,p.key.attached,false));QQuickKeyEvent e;QKeyEvent physical(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier);e.reset(physical);e.setAccepted(false);ownKeys->pressed(&e);C(e.isAccepted()&&presses==1);e.setAccepted(false);ownKeys->released(&e);C(e.isAccepted()&&releases==1);consumed=false;e.setAccepted(true);ownKeys->pressed(&e);C(!e.isAccepted()&&presses==2);closeInCallback=true;ownKeys->released(&e);C(!receiver&&!e.isAccepted()&&releases==2);e.setAccepted(false);ownKeys->pressed(&e);C(presses==2);
 {QQuickWindow win;win.resize(800,600);QQuickItem source(win.contentItem()),destination(win.contentItem());source.setSize(QSizeF(800,600));destination.setSize(QSizeF(800,600));consumed=true;closeInCallback=false;receiver=ScreenKeyReceiver::create(p,sig,&destination,handler,nullptr);C(receiver);ScreenKeyLease chain(p,sig);C(chain.attach(&source,&destination));win.show();source.forceActiveFocus();QCoreApplication::processEvents();C(win.activeFocusItem()==&source);unsigned oldP=presses,oldR=releases;QKeyEvent down(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier);QCoreApplication::sendEvent(&win,&down);C(down.isAccepted()&&presses==oldP+1);QKeyEvent up(QEvent::KeyRelease,Qt::Key_A,Qt::NoModifier);QCoreApplication::sendEvent(&win,&up);C(up.isAccepted()&&releases==oldR+1);C(chain.close());receiver->close();receiver=nullptr;}
 for(unsigned i=0;i<1000;i++){QQuickItem from,to;ScreenKeyLease l(p,sig);C(l.attach(&from,&to));C(l.close());}
 printf("SCREEN_KEYS_PASS assertions=%u real_qt_lists=1 foreign_growth=128 hardware_calls=0\n",checks);
}
