#include "arm_bridge/slot_core.h"
#include <QGuiApplication>
#include <QCoreApplication>
#include <QThread>
#include <QLibrary>
#include <cstdio>
#include <cstdlib>
static int checks,preCalls,callbacks,releases;static bool constructionFinished;
static void(*schedule)(int,int,const void*,UiSlot*);
static void check(bool v,const char*n){++checks;if(!v){fprintf(stderr,"PREROUTINE_FAIL %s\n",n);exit(1);}}
static void call(void*){check(constructionFinished,"queued-after-GUI-construction");++callbacks;}
static void release(void*){++releases;}
static void pre(){++preCalls;auto*app=QCoreApplication::instance();check(app&&app->thread()==QThread::currentThread(),"pre-has-GUI-thread-context");auto*slot=ui_slot_new(call,release,nullptr);check(slot,"slot-allocation");schedule(0,0,app,slot);}
int main(int argc,char**argv){
 auto add=reinterpret_cast<void(*)(void(*)())>(QLibrary::resolve("Qt6Core","_Z14qAddPreRoutinePFvvE"));schedule=reinterpret_cast<decltype(schedule)>(QLibrary::resolve("Qt6Core","_ZN6QTimer14singleShotImplEiN2Qt9TimerTypeEPK7QObjectPN9QtPrivate15QSlotObjectBaseE"));check(add&&schedule,"fixed-Qt-entry-symbols");
 add(pre);check(preCalls==0,"before-app-registration-does-not-run");auto*app=new QGuiApplication(argc,argv);constructionFinished=true;
 check(preCalls==1&&callbacks==0&&releases==0,"pre-only-enqueues-during-construction");app->processEvents();check(callbacks==1&&releases==1,"queued-once-and-released");
 add(pre);check(preCalls==2&&callbacks==1,"after-app-registration-immediate-pre-only");app->processEvents();check(callbacks==2&&releases==2,"after-app-queued-once");
 pre();check(callbacks==2&&releases==2,"pending-before-app-destroy");delete app;check(callbacks==2&&releases==3,"app-destroy-cancels-and-releases");
 printf("NATIVE_PREROUTINE_PASS assertions=%d real_Qt=1 camera_requests=0\n",checks);
}
