#include "animation_attachment.hpp"
#include "native_animation_test_support.hpp"
#include "qt_abi_profile.hpp"
#include "qt_stock_profile.hpp"
#include "qt_menu_profile.hpp"
#include "qt_signal_profile.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QtQuick/private/qquickimage_p.h>
#include <new>
#include <cstring>
using namespace x2dui;using namespace animation_test;
static uint64_t timeNow=1000;static int polls=0,plays=0,soundPolls=0;static AnimationAttachment*closeOnPlay=nullptr;
extern "C" {
void x2d_effect_init(X2dEffectService*s){std::memset(s,0,sizeof *s);s->fd=-1;}
void x2d_effect_set_enabled(X2dEffectService*s,int enabled,uint64_t){s->enabled=enabled;}
void x2d_effect_poll(X2dEffectService*,uint64_t){++polls;}
void x2d_sound_init(X2dSoundService*s){std::memset(s,0,sizeof *s);s->fd=-1;}
void x2d_sound_set_enabled(X2dSoundService*s,int enabled,uint64_t){s->enabled=enabled;if(!enabled)s->ready=0;}
void x2d_sound_poll(X2dSoundService*s,uint64_t){++soundPolls;if(s->enabled)s->ready=1;}
int x2d_sound_play(X2dSoundService*s,uint64_t){++plays;s->saving=1;if(closeOnPlay){auto*a=closeOnPlay;closeOnPlay=nullptr;a->close();}return 1;}
void x2d_sound_destroy(X2dSoundService*s){s->enabled=s->ready=0;s->fd=-1;}
}
struct TestImage final:QQuickImage {explicit TestImage(QQuickItem*p):QQuickImage(p){}using QQuickImage::componentComplete;};
static AnimationImageProfile imageProfile(){AnimationImageProfile p{};p.size=sizeof(TestImage);p.construct=[](void*v,void*q){new(v)TestImage(static_cast<QQuickItem*>(q));};p.complete=[](void*v){static_cast<TestImage*>(v)->componentComplete();};p.set_context=[](void*v,void*c){QQmlEngine::setContextForObject(static_cast<QObject*>(v),static_cast<QQmlContext*>(c));};return p;}
static QQuickItem*make(QQmlEngine&e,QQuickWindow&w){QQmlComponent c(&e);c.setData(R"(import QtQuick
Item {width:800;height:600;objectName:'mainFixture';Item {objectName:'ControlDrawer_root';property string mainState:'liveview';width:800;height:600} }
)",QUrl());if(c.isError())qWarning()<<c.errors();auto*o=qobject_cast<QQuickItem*>(c.create());check(o,"real Qt tree fixture");o->setParentItem(w.contentItem());return o;}
int main(int argc,char**argv){QGuiApplication app(argc,argv);check(argc==3,"arguments");fonts(app,QDir(QString::fromLocal8Bit(argv[1])));DataOnlyFactory network;QQmlEngine engine;engine.setNetworkAccessManagerFactory(&network);QQuickWindow window;window.resize(800,600);window.show();auto*root=make(engine,window);auto*drawer=root->findChild<QQuickItem*>("ControlDrawer_root");check(drawer,"drawer");auto abi=qt_abi_profile();auto api=abi_view_api(&abi);auto stock=qt_stock_profile();auto menu=qt_menu_profile();auto signal=qt_signal_profile();X2dEffectService effect{};x2d_effect_init(&effect);effect.ready=1;effect.mode=0;
 AnimationAttachment a(api,stock,menu,signal,imageProfile(),&effect,{nullptr,[](void*){return timeNow;}});
 auto tick=[&](uint64_t t){check(t>=timeNow,"monotonic test schedule");timeNow=t;return a.tick(root,t,800,600);};
 check(tick(1000)&&a.has_source()&&!a.active(),"discover without menu or animation");check(polls==1&&effect.enabled,"reads preferences without opening menu");check(root->findChild<QObject*>("X2dNativeShutterAnimation")==nullptr,"mode0 no overlay");
 timeNow=1001;check(drawer->setProperty("mainState","exposing"),"source state only fixture");check(!a.active()&&!plays,"mode0 no animation or audio");timeNow=1002;drawer->setProperty("mainState","liveview");effect.mode=1;check(tick(1010),"mode1 attach");settle(80);timeNow=1020;drawer->setProperty("mainState","exposing");check(a.active()&&!plays,"between-ticks exposure signal seen without audio");check(tick(1030),"first frame accepted");settle(80);auto*overlay=root->findChild<QQuickItem*>("X2dNativeShutterAnimation");check(overlay&&overlay->isVisible(),"actual view shows after ready");
 for(int i=0;i<64;i++)check(tick(1040+i)&&a.active()&&!plays,"same exposure no audio or retrigger");
 timeNow=1110;drawer->setProperty("mainState","liveview");check(!a.active(),"falling edge between ticks");check(tick(1120),"hide after exposure");check(!overlay->isVisible(),"stock state hides overlay");
 effect.mode=2;check(tick(1130),"mode2 ready");int before=plays;timeNow=1140;drawer->setProperty("mainState","exposing");check(a.active()&&plays==before+1,"one play on observed edge");for(int i=0;i<64;i++)check(tick(1150+i)&&plays==before+1,"unknown audio completion not replayed");
 effect.mode=0;check(tick(1220)&&!a.active()&&!root->findChild<QObject*>("X2dNativeShutterAnimation"),"disable tears down overlay");effect.mode=2;check(tick(1230)&&!a.active()&&plays==before+1,"enable during same exposing not new edge");
 timeNow=1240;drawer->setProperty("mainState","liveview");timeNow=1241;drawer->setProperty("mainState","exposing");timeNow=1242;drawer->setProperty("mainState","liveview");check(!a.active()&&plays==before+2,"both transitions between ticks observed exactly once");check(tick(1250),"post-edge scheduler");
 auto*duplicate=new QQuickItem(root);duplicate->setObjectName("ControlDrawer_root");check(!tick(1260)&&!a.has_source()&&!a.active(),"multiple sources rejected");delete duplicate;check(tick(1270)&&a.has_source(),"unique source restored");
 closeOnPlay=&a;timeNow=1280;drawer->setProperty("mainState","exposing");check(!a.active()&&!a.has_source()&&!effect.enabled,"synchronous close from sound dispatch");check(!root->findChild<QObject*>("X2dNativeShutterAnimation"),"close removes owned view");
 timeNow=1290;drawer->setProperty("mainState","liveview");check(tick(1300)&&a.has_source(),"explicit later tick reattaches");delete drawer;check(!a.has_source()&&!a.active(),"source deletion");check(tick(1310)&&!a.has_source(),"no source remains passive");delete root;root=nullptr;check(!a.active()&&!a.has_source(),"root deletion observed");check(!tick(1320),"null root refused");a.close();check(polls>0&&soundPolls>0,"explicit service fixtures exercised");
 printf("NATIVE_ANIMATION_ATTACHMENT_PASS assertions=%d sound_plays=%d effect_polls=%d realQt=1 services=explicit_fixture camera_requests=0\n",checks,plays,polls);
}
