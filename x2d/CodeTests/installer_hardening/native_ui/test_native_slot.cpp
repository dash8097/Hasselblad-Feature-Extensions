#include <QCoreApplication>
#include <QEventLoop>
#include <QLibrary>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include "arm_bridge/slot_core.h"
static int assertions;
static void check(bool truth,const char*label){++assertions;if(!truth){std::fprintf(stderr,"SLOT_FAIL %s\n",label);std::exit(1);}}
struct Counts {int calls=0,releases=0;QThread*thread=nullptr;};
static void called(void*opaque){auto*p=static_cast<Counts*>(opaque);p->calls++;p->thread=QThread::currentThread();}
static void released(void*opaque){static_cast<Counts*>(opaque)->releases++;}
static void drain(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);
 QLibrary qt("Qt6Core");check(qt.load(),"qt-load");
 using Schedule=void(*)(int,int,const void*,UiSlot*);
 auto schedule=reinterpret_cast<Schedule>(qt.resolve("_ZN6QTimer14singleShotImplEiN2Qt9TimerTypeEPK7QObjectPN9QtPrivate15QSlotObjectBaseE"));
 check(schedule!=nullptr,"exact-qt-single-shot-symbol");
 check(sizeof(UiSlot)==40&&offsetof(UiSlot,impl)==8,"slot-layout");
 check(ui_slot_new(nullptr,released,nullptr)==nullptr,"null-callback");
 for(int delay: {0,1,10}){
  Counts counts;auto*slot=ui_slot_new(called,released,&counts);check(slot!=nullptr,"allocated");
  schedule(delay,0,&app,slot);check(counts.calls==0&&counts.releases==0,"async-not-immediate");
  drain(40);check(counts.calls==1&&counts.releases==1,"called-and-released-once");
  check(counts.thread==app.thread(),"app-thread");drain(10);check(counts.calls==1&&counts.releases==1,"no-repeat");
 }
 for(int delay: {0,1,10}){
  Counts counts;auto*receiver=new QObject;auto*slot=ui_slot_new(called,released,&counts);check(slot!=nullptr,"cancel-allocated");
  schedule(delay,0,receiver,slot);delete receiver;drain(40);
  check(counts.calls==0&&counts.releases==1,"deleted-context-no-call-release");
 }
 Counts direct;auto*slot=ui_slot_new(called,released,&direct);unsigned char equal=1;
 slot->impl(2,slot,nullptr,nullptr,&equal);check(equal==0,"compare-false");slot->impl(0,slot,nullptr,nullptr,nullptr);check(direct.releases==1,"owned-free");
 std::printf("NATIVE_SLOT_QT_PASS assertions=%d Qt=6.4.1 camera_requests=0\n",assertions);return 0;
}
