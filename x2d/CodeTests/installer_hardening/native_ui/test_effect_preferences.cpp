#include "effect_preferences.hpp"
#include <QtGui/QGuiApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QMetaProperty>
#include <QtCore/QLibrary>
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
static void verify(bool condition,const char *why){++checks;if(!condition){fprintf(stderr,"NATIVE_UI_FAIL %s\n",why);std::exit(1);}}
class FakeTransport final:public EffectTransport {
public:
    int count=0,last=-999;bool automaticFailure=false;
    void request(int mode)override{++count;last=mode;if(automaticFailure)emit completed(503,{});}
    void reply(int status,QByteArray body){emit completed(status,body);}
};
static void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){
    QGuiApplication app(argc,argv);verify(QString::fromLatin1(qVersion())==QStringLiteral("6.4.1"),"exact-Qt-version");
    FakeTransport wire;EffectPreferences item(nullptr,&wire);
    item.start();wait(320);verify(wire.count==0,"disabled-no-initial-or-timer-I/O");
    verify(!item.isVisible()&&!item.ready()&&item.mode()==0&&!item.saving(),"default-state");
    verify(item.labels().size()==3&&item.labels()[1]==QString::fromUtf8("动画"),"labels");
    item.selectMode(2);verify(wire.count==0,"disabled-select-blocked");
    item.setNetworkEnabled(true);wait(320);verify(wire.count==1&&wire.last==-1&&item.saving()&&item.attempts()==1,"retry-read");
    item.selectMode(1);wait(320);verify(wire.count==1,"no-duplicate-pending");
    wire.reply(200,"0\n");verify(!item.ready()&&!item.errorText().isEmpty(),"reject-trailing-newline");
    wait(320);verify(wire.count==2&&wire.last==-1,"retry-after-invalid");wire.reply(200,"1");verify(item.ready()&&item.mode()==1&&item.errorText().isEmpty(),"valid-mode");
    wait(320);verify(wire.count==2,"ready-stops-polling");item.selectMode(3);item.selectMode(-2);verify(wire.count==2,"invalid-choice-blocked");
    verify(QMetaObject::invokeMethod(&item,"selectMode",Qt::DirectConnection,Q_ARG(int,2)),"native-meta-invokable");verify(wire.count==3&&wire.last==2&&item.saving(),"one-post");wire.reply(503,"2");verify(!item.ready()&&item.attempts()==0&&!item.errorText().isEmpty(),"uncertain-write-requires-readback");
    item.selectMode(0);verify(wire.count==3,"block-write-after-uncertain");wait(320);verify(wire.count==4&&wire.last==-1,"uncertain-write-readback");wire.reply(200,"2");verify(item.ready()&&item.mode()==2,"readback-confirms");
    auto*factory=x2d_create_effect_preferences(nullptr,false);verify(factory&&factory->metaObject()->indexOfProperty("mode")>=0,"native-factory-properties");delete factory;
    item.setNetworkEnabled(false);wait(320);verify(wire.count==4,"disable-stops-retry");
    wire.reply(200,"0");verify(item.mode()==2,"ignore-unsolicited-completion");
    FakeTransport unavailable;unavailable.automaticFailure=true;EffectPreferences bounded(nullptr,&unavailable);bounded.setNetworkEnabled(true);bounded.start();bounded.start();wait(10800);verify(unavailable.count==35&&bounded.attempts()==34&&!bounded.saving(),"34-retries-after-one-initial-read");wait(400);verify(unavailable.count==35,"retry-budget-exhausted");
    verify(argc==2,"dll-path-provided");QLibrary library(QString::fromLocal8Bit(argv[1]));verify(library.load(),"load-real-native-dll");using Create=QQuickItem*(*)(QQuickItem*,bool);auto create=reinterpret_cast<Create>(library.resolve("x2d_create_effect_preferences"));verify(create!=nullptr,"resolve-native-factory");auto*loaded=create(nullptr,false);verify(loaded&&loaded->property("mode").toInt()==0&&!loaded->property("networkEnabled").toBool(),"native-dll-object-state");delete loaded;verify(library.unload(),"native-dll-object-destroy-and-unload");
    printf("NATIVE_UI_PASS assertions=%u qt=%s network_requests=0 qml_documents=0\n",checks,qVersion());return 0;
}
